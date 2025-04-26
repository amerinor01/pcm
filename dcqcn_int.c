/* dcqcn.c - DCQCN skeleton (integer rate) */
#include "cc_box_api.h"
#include "dcqcn_params.h"
#include <stdlib.h>

#define DCQCN_FP_SCALE  65536U       /* Q16.16 scale factor */
#define DCQCN_GAMMA_FP  4096U        /* 0.0625 * 2^16 */

typedef struct dcqcn_state {
	uint32_t rate;         /* bytes/sec */
	uint32_t rate_max;     /* bytes/sec */
	uint32_t alpha;        /* congestion estimate in Q16.16 */
	uint32_t k_min;        /* no-mark threshold */
	uint32_t k_max;        /* full-mark threshold */
} dcqcn_state_t;

static cc_state_t *
dcqcn_init(void *params)
{
	dcqcn_state_t *s = malloc(sizeof(*s));
	dcqcn_params_t *p = (dcqcn_params_t *)params;
	if (!s)
		return NULL;

	s->rate_max = p ? p->rate_max : 100000000U;
	s->rate     = s->rate_max;
	s->alpha    = 0;
	s->k_min    = p ? p->k_min    : 50 * 1024U;
	s->k_max    = p ? p->k_max    : 100 * 1024U;
	return (cc_state_t *)s;
}

static void
dcqcn_cleanup(cc_state_t *st)
{
	free(st);
}

static void
dcqcn_on_event(cc_state_t *st, enum cc_event_t evt, uint32_t val)
{
	dcqcn_state_t *s = (dcqcn_state_t *)st;
	uint32_t p_fp;

	switch (evt) {
	case CC_EVT_ECN:
		if (val < s->k_min)
			p_fp = 0;
		else if (val >= s->k_max)
			p_fp = DCQCN_FP_SCALE;
		else
			p_fp = (uint32_t)(((uint64_t)(val - s->k_min) * DCQCN_FP_SCALE) / (s->k_max - s->k_min));

		s->alpha = (uint32_t)((((uint64_t)(DCQCN_FP_SCALE - DCQCN_GAMMA_FP) * s->alpha) +
		                 (uint64_t)DCQCN_GAMMA_FP * p_fp) >> 16);

		/* rate *= (1 - alpha/2) */
		{
			uint32_t alpha_half = s->alpha >> 1;
			uint32_t factor = DCQCN_FP_SCALE - alpha_half;
			s->rate = (uint32_t)(((uint64_t)s->rate * factor) >> 16);
		}

		if (s->rate > s->rate_max)
			s->rate = s->rate_max;
		break;

	case CC_EVT_ACK:
		/* additive increase: +1% per ACK */
		s->rate += s->rate / 100U;
		if (s->rate > s->rate_max)
			s->rate = s->rate_max;
		break;

	case CC_EVT_NACK:
	case CC_EVT_TIMEOUT:
		/* multiplicative decrease */
		s->rate >>= 1;
		break;

	default:
		break;
	}
}

static uint32_t
dcqcn_get_rate(cc_state_t *st)
{
	return ((struct dcqcn_state *)st)->rate;
}

const cc_algo_t dcqcn_algo = {
	.init      = dcqcn_init,
	.cleanup   = dcqcn_cleanup,
	.on_event  = dcqcn_on_event,
	.get_rate  = dcqcn_get_rate,
	.set_param = NULL,
};