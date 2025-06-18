#define DCTCP_MAX_ALPHA	1024U

struct dctcp {
	u32 old_delivered;
	u32 old_delivered_ce;
	u32 prior_rcv_nxt;
	u32 dctcp_alpha;
	u32 next_seq;
	u32 ce_state;
	u32 loss_cwnd;
	struct tcp_plb_state plb;
};

static inline void dctcp_ece_ack_cwr(struct sock *sk, u32 ce_state)
{
	struct tcp_sock *tp = tcp_sk(sk);

	if (ce_state == 1)
		tp->ecn_flags |= TCP_ECN_DEMAND_CWR;
	else
		tp->ecn_flags &= ~TCP_ECN_DEMAND_CWR;
}

/* Minimal DCTP CE state machine:
 *
 * S:	0 <- last pkt was non-CE
 *	1 <- last pkt was CE
 */
static inline void dctcp_ece_ack_update(struct sock *sk, enum tcp_ca_event evt,
					u32 *prior_rcv_nxt, u32 *ce_state)
{
	u32 new_ce_state = (evt == CA_EVENT_ECN_IS_CE) ? 1 : 0;

	if (*ce_state != new_ce_state) {
		/* CE state has changed, force an immediate ACK to
		 * reflect the new CE state. If an ACK was delayed,
		 * send that first to reflect the prior CE state.
		 */
		if (inet_csk(sk)->icsk_ack.pending & ICSK_ACK_TIMER) {
			dctcp_ece_ack_cwr(sk, *ce_state);
			__tcp_send_ack(sk, *prior_rcv_nxt, 0);
		}
		inet_csk(sk)->icsk_ack.pending |= ICSK_ACK_NOW;
	}
	*prior_rcv_nxt = tcp_sk(sk)->rcv_nxt;
	*ce_state = new_ce_state;
	dctcp_ece_ack_cwr(sk, new_ce_state);
}

static unsigned int dctcp_shift_g __read_mostly = 4; /* g = 1/2^4 */

static int dctcp_shift_g_set(const char *val, const struct kernel_param *kp)
{
	return param_set_uint_minmax(val, kp, 0, 10);
}

static void dctcp_reset(const struct tcp_sock *tp, struct dctcp *ca)
{
	ca->next_seq = tp->snd_nxt;

	ca->old_delivered = tp->delivered;
	ca->old_delivered_ce = tp->delivered_ce;
}

__bpf_kfunc static void dctcp_init(struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);

	if (tcp_ecn_mode_any(tp) ||
	    (sk->sk_state == TCP_LISTEN ||
	     sk->sk_state == TCP_CLOSE)) {
		struct dctcp *ca = inet_csk_ca(sk);

		ca->prior_rcv_nxt = tp->rcv_nxt;

		ca->dctcp_alpha = min(dctcp_alpha_on_init, DCTCP_MAX_ALPHA);

		ca->loss_cwnd = 0;
		ca->ce_state = 0;

		dctcp_reset(tp, ca);
		tcp_plb_init(sk, &ca->plb);

		return;
	}

	/* No ECN support? Fall back to Reno. Also need to clear
	 * ECT from sk since it is set during 3WHS for DCTCP.
	 */
	inet_csk(sk)->icsk_ca_ops = &dctcp_reno;
	INET_ECN_dontxmit(sk);
}

__bpf_kfunc static u32 dctcp_ssthresh(struct sock *sk)
{
	struct dctcp *ca = inet_csk_ca(sk);
	struct tcp_sock *tp = tcp_sk(sk);

	ca->loss_cwnd = tcp_snd_cwnd(tp);
	return max(tcp_snd_cwnd(tp) - ((tcp_snd_cwnd(tp) * ca->dctcp_alpha) >> 11U), 2U);
}

__bpf_kfunc static void dctcp_update_alpha(struct sock *sk, u32 flags)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	struct dctcp *ca = inet_csk_ca(sk);

	/* Expired RTT */
	if (!before(tp->snd_una, ca->next_seq)) {
		u32 delivered = tp->delivered - ca->old_delivered;
		u32 delivered_ce = tp->delivered_ce - ca->old_delivered_ce;
		u32 alpha = ca->dctcp_alpha;
		u32 ce_ratio = 0;

		if (delivered > 0) {
			/* dctcp_alpha keeps EWMA of fraction of ECN marked
			 * packets. Because of EWMA smoothing, PLB reaction can
			 * be slow so we use ce_ratio which is an instantaneous
			 * measure of congestion. ce_ratio is the fraction of
			 * ECN marked packets in the previous RTT.
			 */
			if (delivered_ce > 0)
				ce_ratio = (delivered_ce << TCP_PLB_SCALE) / delivered;
			tcp_plb_update_state(sk, &ca->plb, (int)ce_ratio);
			tcp_plb_check_rehash(sk, &ca->plb);
		}

		/* alpha = (1 - g) * alpha + g * F */

		alpha -= min_not_zero(alpha, alpha >> dctcp_shift_g);
		if (delivered_ce) {

			/* If dctcp_shift_g == 1, a 32bit value would overflow
			 * after 8 M packets.
			 */
			delivered_ce <<= (10 - dctcp_shift_g);
			delivered_ce /= max(1U, delivered);

			alpha = min(alpha + delivered_ce, DCTCP_MAX_ALPHA);
		}
		/* dctcp_alpha can be read from dctcp_get_info() without
		 * synchro, so we ask compiler to not use dctcp_alpha
		 * as a temporary variable in prior operations.
		 */
		WRITE_ONCE(ca->dctcp_alpha, alpha);
		dctcp_reset(tp, ca);
	}
}

static void dctcp_react_to_loss(struct sock *sk)
{
	struct dctcp *ca = inet_csk_ca(sk);
	struct tcp_sock *tp = tcp_sk(sk);

	ca->loss_cwnd = tcp_snd_cwnd(tp);
	tp->snd_ssthresh = max(tcp_snd_cwnd(tp) >> 1U, 2U);
}

__bpf_kfunc static void dctcp_state(struct sock *sk, u8 new_state)
{
	if (new_state == TCP_CA_Recovery &&
	    new_state != inet_csk(sk)->icsk_ca_state)
		dctcp_react_to_loss(sk);
	/* We handle RTO in dctcp_cwnd_event to ensure that we perform only
	 * one loss-adjustment per RTT.
	 */
}

__bpf_kfunc static void dctcp_cwnd_event(struct sock *sk, enum tcp_ca_event ev)
{
	struct dctcp *ca = inet_csk_ca(sk);

	switch (ev) {
	case CA_EVENT_ECN_IS_CE:
	case CA_EVENT_ECN_NO_CE:
		dctcp_ece_ack_update(sk, ev, &ca->prior_rcv_nxt, &ca->ce_state);
		break;
	case CA_EVENT_LOSS:
		tcp_plb_update_state_upon_rto(sk, &ca->plb);
		dctcp_react_to_loss(sk);
		break;
	case CA_EVENT_TX_START:
		tcp_plb_check_rehash(sk, &ca->plb); /* Maybe rehash when inflight is 0 */
		break;
	default:
		/* Don't care for the rest. */
		break;
	}
}

__bpf_kfunc static u32 dctcp_cwnd_undo(struct sock *sk)
{
	const struct dctcp *ca = inet_csk_ca(sk);
	struct tcp_sock *tp = tcp_sk(sk);

	return max(tcp_snd_cwnd(tp), ca->loss_cwnd);
}

static struct tcp_congestion_ops dctcp __read_mostly = {
	.init		= dctcp_init,
	.in_ack_event   = dctcp_update_alpha,
	.cwnd_event	= dctcp_cwnd_event,
	.ssthresh	= dctcp_ssthresh,
	.cong_avoid	= tcp_reno_cong_avoid,
	.undo_cwnd	= dctcp_cwnd_undo,
	.set_state	= dctcp_state,
	.get_info	= dctcp_get_info,
	.flags		= TCP_CONG_NEEDS_ECN,
	.owner		= THIS_MODULE,
	.name		= "dctcp",
};