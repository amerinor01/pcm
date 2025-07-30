#include <assert.h>

#include "dctcp.h"
#include "tcp_utils.h"

#define DCTCP_SSTHRESH(cur_cwnd) (MAX(cur_cwnd * (1.0 - get_var_float(VAR_ALPHA) / 2.0), 2U))
FAST_RECOVERY_DEFINE(dctcp, DCTCP_SSTHRESH);

static PCM_FORCE_INLINE void dctcp_alpha_update(ALGO_CTX_ARGS, pcm_uint num_acks) {
    pcm_uint delivered = get_var_uint(VAR_EPOCH_DELIVERED) + num_acks;
    pcm_uint delivered_ecn = get_var_uint(VAR_EPOCH_ECN_DELIVERED) + get_signal(SIG_ECN);

    /*
     * Note: Linux DCTCP detects it based on reaching seq number that at the
     * window boundary. We do this simpler in expectation that receiving cwnd of
     * ACKs takes RTT.
     *
     * https://github.com/torvalds/linux/blob/aef17cb3d3c43854002956f24c24ec8e1a0e3546/net/ipv4/tcp_dctcp.c#L132
     */
    if (delivered >= get_var_uint(VAR_EPOCH_TO_DELIVER)) {
        // alpha = (1 - g) * alpha + g * F
        pcm_float F = (pcm_float)delivered_ecn / (pcm_float)delivered;
        set_var_float(VAR_ALPHA, (1 - GAMMA) * get_var_float(VAR_ALPHA) + GAMMA * F);
        set_var_uint(VAR_EPOCH_DELIVERED, 0);
        set_var_uint(VAR_EPOCH_ECN_DELIVERED, 0);
        update_signal(SIG_ECN, -get_signal(SIG_ECN));
        /* TODO: support LB path change */
    } else {
        set_var_uint(VAR_EPOCH_DELIVERED, delivered);
        set_var_uint(VAR_EPOCH_ECN_DELIVERED, delivered_ecn);
    }

    // fprintf(stderr, "DCTCP: delivered=%llu ecned=%llu to_deliver=%llu alpha=%lf\n",
    //          delivered, delivered_ecn,
    //          get_var_uint(VAR_EPOCH_TO_DELIVER),
    //          get_var_float(VAR_ALPHA));
}

int algorithm_main() {
    pcm_uint cur_cwnd = get_control(CTRL_CWND) / MSS;
    pcm_uint num_acks = get_signal(SIG_ACK);

    /*
     * Negative feedback part has higher priority
     */
    pcm_uint acks_to_consume = 0;

    if (get_signal(SIG_NACK) > 0) {
        dctcp_fast_recovery(ALGO_CTX_PASS, &cur_cwnd);
        update_signal(SIG_NACK, -1);
        goto save_cwnd_and_exit;
    }

    if (get_signal(SIG_RTO) > 0) {
        tcp_timeout_recovery(ALGO_CTX_PASS, &cur_cwnd);
        update_signal(SIG_RTO, -1);
        goto save_cwnd_and_exit;
    }

    /*
     * We have no positive feedback and at least one ACK (otherwise we wouldn't
     * be triggered)
     */
    assert(get_signal(SIG_ACK));
    acks_to_consume = num_acks;

    if (get_var(VAR_IN_FAST_RECOV) && acks_to_consume > 0)
        tcp_fast_recovery_exit(ALGO_CTX_PASS, &cur_cwnd, &acks_to_consume);

    /*
     * Fallback to rate increase
     */
    if (!get_var(VAR_IN_FAST_RECOV) && acks_to_consume > 0) {
        if (cur_cwnd < get_var(VAR_SSTHRESH)) {
            tcp_slow_start(ALGO_CTX_PASS, &cur_cwnd, &acks_to_consume);
        }
        if (acks_to_consume) {
            tcp_cong_avoid(ALGO_CTX_PASS, &cur_cwnd, &acks_to_consume);
        }
    }

save_cwnd_and_exit:
    // fprintf(stderr,
    //          "cur_cwnd=%llu ssthresh=%llu in_fr=%llu num_acks=%llu "
    //          "acks_to_consume=%llu\n",
    //          cur_cwnd, get_var_uint(VAR_SSTHRESH),
    //          get_var_uint(VAR_IN_FAST_RECOV), num_acks, acks_to_consume);
    update_signal(SIG_ACK, -(num_acks - acks_to_consume));
    set_control(CTRL_CWND, cur_cwnd * MSS);

    dctcp_alpha_update(ALGO_CTX_PASS, num_acks);

    return PCM_SUCCESS;
}