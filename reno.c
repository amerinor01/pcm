/* reno.c - TCP Reno skeleton with integer cwnd/rate */
#include "cc_box_api.h"
#include <stdlib.h>
#include <limits.h>

#define max(a,b) \
({ __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
  _a > _b ? _a : _b; })

typedef struct {
	uint32_t cwnd;      /* in MSS units */
	uint32_t ssthresh;  /* in MSS units */
	uint32_t acked;     /* ACK counter for linear phase */
} reno_state_t;

static cc_state_t *
reno_init(void *params)
{
    (void)params;

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

static inline void
reno_update_ssthresh(reno_state_t *s)
{
	s->ssthresh = max((s->cwnd) >> 1U, 2U);
}

static void
reno_on_event(cc_state_t *st, enum cc_event_t evt)
{
	reno_state_t *s = (reno_state_t *)st;

	switch (evt) {
    case CC_EVT_ECN:
    	/* ECN ignored in classic Reno: fallthrough to ACK */

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
		/* fast retransmit on three dup-acks ala RFC 5681: 
         * do multiplicative decrease: halve slow-start threshold and set cwnd to it */
		reno_update_ssthresh(s);
		s->cwnd  = s->ssthresh;
		s->acked = 0U;
		break;
	
	case CC_EVT_TIMEOUT:
		/* From RFC 5681 par 3:
         * After retransmitting the dropped segment the TCP sender uses the slow start
         * algorithm to increase the window from 1 full-sized segment to the new
         * value of ssthresh, at which point congestion avoidance again takes over */
		reno_update_ssthresh(s);
		s->cwnd  = 1U;
		s->acked = 0U;
		break;

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