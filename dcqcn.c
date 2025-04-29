/* dcqcn.c - DCQCN skeleton (integer rate) */
#include "cc_box_api.h"
#include "dcqcn_params.h"
#include <stdlib.h>
#include <stdio.h>

#define DCQCN_FLOW_RATE_MAX 100000000U
#define DCQCN_BYTE_COUNTER (4096 * 32)
#define DCQCN_RATE_INCREASE_TIMER 1000
#define DCQCN_GAMMA 0.00390625 /* 1/256 */
#define DCQCN_ALPHA_INIT 1.0
#define DCQCN_ALPHA_TIMER 55
#define DCQCN_RAI (100000U)
#define DCQCN_RHAI (1000000U)
#define DCQCN_FR_STEPS 5

typedef struct dcqcn_state {
    uint32_t byte_counter;        /* NIC trigger when reached */
    uint32_t rate_increase_timer; /* NIC trigger when expires */
    uint32_t alpha_timer;         /* NIC trigger when expires */ 
    uint32_t rate_increase_timer_evts;
    uint32_t byte_counter_evts;
    uint32_t rate_max;      /* maximum rate */
    uint32_t rate_cur;      /* current rate */
    uint32_t rate_tgt;      /* saved pre-cutback rate */
    double   alpha;         /* fraction of ECN'ed packets */
} dcqcn_state_t;

static cc_state_t *
dcqcn_init(void *params)
{
	dcqcn_params_t *p = (dcqcn_params_t *)params;

    dcqcn_state_t *s = malloc(sizeof(*s));
	if (!s)
		return NULL;

    /* Initialize flow state */
    s->rate_increase_timer_evts = s->byte_counter_evts = 0;
	s->rate_max = p ? p->rate_max : DCQCN_FLOW_RATE_MAX;
    s->rate_cur = s->rate_max;
    s->rate_tgt = s->rate_max;
	s->alpha = DCQCN_ALPHA_INIT;

    /* Set NIC triggers */
    s->byte_counter = DCQCN_BYTE_COUNTER;
    s->rate_increase_timer = DCQCN_RATE_INCREASE_TIMER;
    s->alpha_timer = DCQCN_ALPHA_TIMER;

    return (cc_state_t *)s;
}

static void
dcqcn_cleanup(cc_state_t *st)
{
	free(st);
}

static inline void
dcqcn_rate_decrease(dcqcn_state_t *s)
{
    /* record old rate as target rate */
    s->rate_tgt = s->rate_cur;
    /* multiplicative decrease factor */
    s->rate_cur *= (1 - 0.5 * s->alpha);
    /* update alpha */
    s->alpha = (1 - DCQCN_GAMMA) * s->alpha + DCQCN_GAMMA;
    //printf("alpha=%f\n", s->alpha);
}

static inline void
dcqcn_rate_increase(dcqcn_state_t *s)
{
    if (s->rate_cur >= s->rate_max)
        return;

    uint32_t min_counter;
    if (max(s->rate_increase_timer_evts, s->byte_counter_evts) < DCQCN_FR_STEPS) {
        /* Fast Recovery */
        //printf("Fast recovery\n");
        //s->rate_tgt = s->rate_cur;
    } else if ((min_counter = min(s->rate_increase_timer_evts, s->byte_counter_evts)) > DCQCN_FR_STEPS) {
        /* Hyper Increase (optional?) */
        //printf("Hyper increase\n");
        s->rate_tgt += min_counter * DCQCN_RHAI;
    } else {
        /* Additive Increase */
        //printf("Additive increase\n");
        s->rate_tgt += DCQCN_RAI;
    }

    s->rate_cur = (s->rate_tgt + s->rate_cur) / 2;

    if (s->rate_cur > s->rate_max)
        s->rate_cur = s->rate_max;
}

static void
dcqcn_on_event(cc_state_t *st, enum cc_event_t evt)
{
	dcqcn_state_t *s = (dcqcn_state_t *)st;

    switch (evt) {

    case CC_EVT_ECN:
        /* React to receiving CNP */
        s->rate_increase_timer_evts = s->byte_counter_evts = 0;
        dcqcn_rate_decrease(s);
        break;

    case CC_EVT_ALPHA_TIMER:
        /* s->alpha_timer expired */
        s->alpha = (1 - DCQCN_GAMMA) * s->alpha;
        /* set trigger again */
        s->alpha_timer = DCQCN_ALPHA_TIMER;
        break;

    case CC_EVT_RATE_INCREASE_TIMER:
        /* s->rate_increase_timer expired */
        s->rate_increase_timer_evts++;
        dcqcn_rate_increase(s);
        /* This timer should be set again after rate is increased here */
        s->rate_increase_timer = DCQCN_RATE_INCREASE_TIMER;
        break;

    case CC_EVT_BYTE_COUNTER:
        /* s->byte_counter reached */
        s->byte_counter_evts++;
        dcqcn_rate_increase(s);
        /* This trigger should be set again after rate is increased */
        s->byte_counter = DCQCN_BYTE_COUNTER;
        break;

    default:
        break;
    }
}

static uint32_t
dcqcn_get_rate(cc_state_t *st)
{
	return ((dcqcn_state_t *)st)->rate_cur;
}

const cc_algo_t dcqcn_algo = {
	.init      = dcqcn_init,
	.cleanup   = dcqcn_cleanup,
	.on_event  = dcqcn_on_event,
	.get_rate  = dcqcn_get_rate,
	.set_param = NULL,
};