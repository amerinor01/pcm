/* reno.c - TCP Reno skeleton with integer cwnd/rate */
#include "cc_box_api.h"
#include <stdlib.h>
#include <limits.h>

typedef struct {
	uint32_t cwnd;      /* in MSS units */
	uint32_t ssthresh;  /* in MSS units */
	uint32_t acked;     /* ACK counter for linear phase */
} reno_state_t;

/* halve cwnd into ssthresh, floor at 2 MSS */
static void
reno_update_ssthresh(reno_state_t *s)
{
	uint32_t half = s->cwnd >> 1;
	s->ssthresh   = (half < 2 ? 2 : half);
}

static cc_state_t *
reno_init(void *params)
{
	reno_state_t *s = malloc(sizeof(*s));
	if (!s)
		return NULL;

	s->cwnd      = 1U;           /* start at 1 MSS */
	s->ssthresh  = UINT32_MAX;   /* infinite initial threshold */
	s->acked     = 0U;
	return (cc_state_t *)s;
}

static void
reno_cleanup(cc_state_t *st)
{
	free(st);
}

static void
reno_on_event(cc_state_t *st, enum cc_event_t evt, uint32_t val)
{
	reno_state_t *s = (reno_state_t *)st;

	switch (evt) {
	case CC_EVT_ACK:
		if (s->cwnd < s->ssthresh) {
			/* slow start: +1 MSS per ACK */
			s->cwnd++;
		} else {
			/* congestion avoidance: +1 MSS per cwnd ACKs */
			s->acked++;
			if (s->acked >= s->cwnd) {
				s->cwnd++;
				s->acked = 0U;
			}
		}
		break;

	case CC_EVT_NACK:
		/* fast retransmit */
		reno_update_ssthresh(s);
		s->cwnd  = s->ssthresh;
		s->acked = 0U;
		break;

	case CC_EVT_TIMEOUT:
		/* RTO: slow-start restart */
		reno_update_ssthresh(s);
		s->cwnd  = 1U;
		s->acked = 0U;
		break;
    
    case CC_EVT_ECN:
    	/* ECN ignored in classic Reno: fallthrough */
	default:
		break;
	}
}

static uint32_t
reno_get_rate(cc_state_t *st)
{
	return ((reno_state_t *)st)->cwnd;
}

const cc_algo_t reno_algo = {
	.init      = reno_init,
	.cleanup   = reno_cleanup,
	.on_event  = reno_on_event,
	.get_rate  = reno_get_rate,
	.set_param = NULL,
};