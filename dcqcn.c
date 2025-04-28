/* dcqcn.c - DCQCN skeleton (integer rate) */
#include "cc_box_api.h"
#include "dcqcn_params.h"
#include <stdlib.h>

typedef struct dcqcn_state {
	uint32_t rate;
	uint32_t rate_max;
	double   alpha;
	double   gamma;
} dcqcn_state_t;

static cc_state_t *
dcqcn_init(void *params)
{
	dcqcn_params_t *p = (dcqcn_params_t *)params;

    dcqcn_state_t *s = malloc(sizeof(*s));
	if (!s)
		return NULL;

	s->rate_max = p ? p->rate_max : 100000000U;
	s->rate     = s->rate_max;
	s->alpha    = 0.0;
	s->gamma    = 0.0625;
	return (cc_state_t *)s;
}

static void
dcqcn_cleanup(cc_state_t *st)
{
	free(st);
}

static void
dcqcn_on_event(cc_state_t *st, enum cc_event_t evt)
{
	dcqcn_state_t *s = (dcqcn_state_t *)st;

    switch (evt) {
    case CC_EVT_ECN:
        /* update alpha via EWMA */
        s->alpha = (1 - s->gamma) * s->alpha + s->gamma;
        /* multiplicative decrease factor = (1 - 0.5*alpha) */
        s->rate  *= (1 - 0.5 * s->alpha);
        if (s->rate > s->rate_max)
            s->rate = s->rate_max;
        break;

    case CC_EVT_ACK:
        /* additive increase: +1% per ACK */
        s->rate += 0.01 * s->rate;
        if (s->rate > s->rate_max)
            s->rate = s->rate_max;
        break;

    case CC_EVT_NACK:
    case CC_EVT_TIMEOUT:
        /* multiplicative decrease */
        s->rate *= 0.5;
        break;

    default:
        break;
    }
}

static uint32_t
dcqcn_get_rate(cc_state_t *st)
{
	return ((dcqcn_state_t *)st)->rate;
}

const cc_algo_t dcqcn_algo = {
	.init      = dcqcn_init,
	.cleanup   = dcqcn_cleanup,
	.on_event  = dcqcn_on_event,
	.get_rate  = dcqcn_get_rate,
	.set_param = NULL,
};