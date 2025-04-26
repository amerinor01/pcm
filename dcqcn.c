/* dcqcn.c - DCQCN skeleton (integer rate) */
#include "cc_box_api.h"
#include "dcqcn_params.h"
#include <stdlib.h>

struct dcqcn_state {
	uint32_t rate;
	uint32_t rate_max;
	double   alpha;
	double   gamma;
	uint32_t k_min;
	uint32_t k_max;
};

static cc_state_t *
dcqcn_init(void *params)
{
	struct dcqcn_state *s = malloc(sizeof(*s));
	dcqcn_params_t *p    = (dcqcn_params_t *)params;

	if (!s)
		return NULL;

	s->rate_max = p ? p->rate_max : 100000000U;
	s->rate     = s->rate_max;
	s->alpha    = 0.0;
	s->gamma    = 0.0625;
	s->k_min    = p ? p->k_min : 50 * 1024U;
	s->k_max    = p ? p->k_max : 100 * 1024U;
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
	struct dcqcn_state *s = (struct dcqcn_state *)st;
	double p;

	switch (evt) {
	case CC_EVT_ECN:
		if (val < s->k_min)
			p = 0.0;
		else if (val >= s->k_max)
			p = 1.0;
		else
			p = (double)(val - s->k_min) / (s->k_max - s->k_min);

		s->alpha = (1 - s->gamma) * s->alpha + s->gamma * p;
		s->rate  = (uint32_t)(s->rate * (1 - 0.5 * s->alpha));
		if (s->rate > s->rate_max)
			s->rate = s->rate_max;
		break;

	case CC_EVT_ACK:
		s->rate += s->rate / 100U;
		if (s->rate > s->rate_max)
			s->rate = s->rate_max;
		break;

	case CC_EVT_NACK:
	case CC_EVT_TIMEOUT:
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