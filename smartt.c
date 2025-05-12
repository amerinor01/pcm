/* smartt.c – TCP smartt sender control loop */
#include "cc_box_api.h"
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <stdbool.h>

#define MTU               4096         /* bytes */
#define BDP               (900 * 1024) /* bytes, example for 100Gbps@12us RTT */
#define MIN_CWND_BYTES    MTU          /* minimum cwnd = 1 MTU */
#define MAX_CWND_BYTES    (1.25 * BDP) /* max cwnd = 1.25 BDP */

/* smartt parameters */

#define TRTT_FACTOR       1.5        /* target RTT factor over base RTT */
#define BASE_RTT          12 /* us */
#define TARGET_RTT        ((uint64_t)(BASE_RTT * TRTT_FACTOR))
#define PI_CONST_COMP(brtt,trtt)  ((brtt)/(trtt - brtt)) /* proportional increase */
#define PI_CONST PI_CONST_COMP(BASE_RTT, TARGET_RTT)
#define MD_CONST          0.8        /* multiplicative decrease constant */
#define FI_CONST          0.25       /* fair increase constant */
#define K_CONST           2          /* fast increase constant */
#define QA_SCALING        0.8        /* QuickAdapt scaling factor */
#define QA_DEADLINE       (TRTT_FACTOR * BASE_RTT)

typedef struct {
    uint64_t acked_bytes;        /* bytes acked since last QA */
    uint64_t bytes_ignored;      /* QA-ignore counter */
    uint64_t bytes_to_ignore;    /* QA-ignore threshold */
    bool trigger_qa;          /* QA active */
    uint64_t qa_deadline;      /* timestamp to limit QA frequency */
    uint64_t fast_count;         /* accumulator for fast increase */
    bool fast_active;        /* fast increase mode active */
    volatile uint64_t last_rtt;         /* last measured RTT in us */
    volatile bool     last_pkt_ecn;     /* ECN mark of last packet */
    volatile uint64_t last_pkt_size;    /* size of last packet in bytes */
    volatile uint64_t avg_rtt;          /* average RTT in us */
    volatile uint64_t now;              /* current timestamp */
    volatile uint64_t cwnd;               /* congestion window in bytes */
} smartt_state_t;

static cc_state_t *
smartt_init(void *params)
{
    smartt_state_t *s = malloc(sizeof(*s));
    if (!s) return NULL;
    s->acked_bytes = 0;
    s->bytes_ignored = 0;
    s->bytes_to_ignore = 0;
    s->trigger_qa = false;
    s->qa_deadline = 0;
    s->fast_count = 0;
    s->fast_active = false;
    // NIC state initialization
    s->last_rtt = BASE_RTT;
    s->avg_rtt = BASE_RTT;
    s->last_pkt_ecn = false;
    s->last_pkt_size = 0;
    s->now = 0;
    s->cwnd = MTU;
    return (cc_state_t *)s;
}

static void
smartt_cleanup(cc_state_t *st)
{
    free(st);
}

static bool quick_adapt(smartt_state_t *s)
{
    bool adapted = false;
    if (s->now >= s->qa_deadline) {
        if (s->trigger_qa) {
            s->trigger_qa = false;
            adapted = true;
            s->cwnd = (uint64_t)(max(s->acked_bytes, MTU) * QA_SCALING);
            s->bytes_to_ignore = s->cwnd;
            s->bytes_ignored = 0;
        }
        s->qa_deadline = s->now + (uint64_t)QA_DEADLINE;
        s->acked_bytes = 0;
    }
    return adapted;
}

static bool fast_increase(smartt_state_t *s)
{
    if (fabs((double)s->last_rtt - BASE_RTT) < 1e-6 && !s->last_pkt_ecn) {
        s->fast_count += s->last_pkt_size;
        if (s->fast_count > s->cwnd || s->fast_active) {
            s->cwnd += (uint64_t)(K_CONST * MTU);
            s->fast_active = true;
        }
    } else {
        s->fast_count = 0;
        s->fast_active = false;
    }
    return s->fast_active;
}

static void core_cases(smartt_state_t *s)
{
    if (s->last_pkt_ecn && s->last_rtt <= TARGET_RTT) {
        // TBD: request LB path change
    } else if (s->last_pkt_ecn && s->last_rtt > TARGET_RTT) {
        /* Multiplicative Decrease */
        double mdf = 1.0 - (s->avg_rtt - TARGET_RTT) / s->avg_rtt * MD_CONST;
        s->cwnd = (uint64_t)((double)s->cwnd * fmax(0.5, mdf));
    } else if (!s->last_pkt_ecn && s->last_rtt > TARGET_RTT) {
        /* Fair Increase */
        s->cwnd += (s->last_pkt_size / s->cwnd) * (uint64_t)(MTU * FI_CONST);
    } else if (!s->last_pkt_ecn && s->last_rtt <= TARGET_RTT) {
        /* Proportional Increase */
        uint64_t increase = ((TARGET_RTT - s->last_rtt) / s->last_rtt) *
                            s->last_pkt_size / s->cwnd * (uint64_t)(MTU * PI_CONST);
        s->cwnd += min(s->last_pkt_size, increase);
    }
}

static void
smartt_on_event(cc_state_t *st, enum cc_event_t evt)
{
    smartt_state_t *s = (smartt_state_t *)st;
    s->acked_bytes += s->last_pkt_size;

    switch (evt) {
    case CC_EVT_ACK:
        if (s->bytes_ignored < s->bytes_to_ignore) {
            s->bytes_ignored += s->last_pkt_size;
            break;
        }

        if (quick_adapt(s) || fast_increase(s))
            break;

        core_cases(s);
        break;

    case CC_EVT_NACK:
    case CC_EVT_TRIMMED:
    case CC_EVT_TIMEOUT:
        s->cwnd -= s->last_pkt_size;
        s->trigger_qa = true;
        // TBD: SMaRTT paper explicitly mentions that this paper needs to be retransmistted here
        // we need to handle this outside
        if (s->bytes_ignored >= s->bytes_to_ignore)
            quick_adapt(s);
        break;

    default:
        return;
    }

    s->cwnd = fmax(fmin(s->cwnd, MAX_CWND_BYTES), MIN_CWND_BYTES);
}

static uint32_t
smartt_get_rate(cc_state_t *st)
{
    smartt_state_t *s = (smartt_state_t *)st;
    return (uint32_t)floor(s->cwnd / MTU);
}

const cc_algo_t smartt_algo = {
    .init      = smartt_init,
    .cleanup   = smartt_cleanup,
    .on_event  = smartt_on_event,
    .get_rate  = smartt_get_rate,
    .set_param = NULL,
};