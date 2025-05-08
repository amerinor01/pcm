/* smartt.c – TCP smartt sender control loop */
#include "cc_box_api.h"
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <stdbool.h>

#define MTU               4096.0     /* bytes */
#define BDP               (900 * 1024) /* bytes, example for 100Gbps@12us RTT */
#define MIN_CWND_BYTES    MTU        /* minimum cwnd = 1 MTU */
#define MAX_CWND_BYTES    (1.25 * BDP) /* max cwnd = 1.25 BDP */

/* smartt parameters (tuned for 100Gbps, 4KiB MTU) */
#define FD_CONST          0.8        /* fair decrease constant */
#define MD_CONST          2.0        /* multiplicative decrease constant */
#define FI_CONST          0.25       /* fair increase constant */
#define MI_CONST(brtt,trtt)  ((brtt)/(trtt - brtt)) /* multiplicative increase */
#define K_CONST           2          /* fast increase constant */
#define QA_SCALING        0.8        /* QuickAdapt scaling factor */
#define TRTT_FACTOR       1.5        /* target RTT factor over base RTT */
#define WTD_ALPHA         0.125      /* EMA alpha for Wait to Decrease */
#define WTD_THRESH        0.25       /* threshold for ECN fraction */

typedef struct {
    double cwnd;               /* congestion window in bytes */
    double cwnd_prev;
    double acked_bytes;        /* bytes acked since last QA */
    double bytes_ignored;      /* QA-ignore counter */
    double bytes_to_ignore;    /* QA-ignore threshold */
    bool   trigger_qa;
    uint64_t qa_deadline;      /* timestamp to limit QA frequency */
    double wtd_avg;            /* EMA of ECN marks */
    double fast_count;
    bool   fast_active;
    uint32_t retrans_cnt;
    uint32_t n_pkts_acked;
    uint64_t last_decrease_time;
    uint64_t base_rtt;         /* in us */
    uint64_t last_rtt;         /* in us */
    uint64_t last_delay;       /* one-way delay component */
    uint64_t now;
} smartt_state_t;

static cc_state_t *
smartt_init(void *params)
{
    smartt_state_t *s = malloc(sizeof(*s));
    if (!s) return NULL;
    /* initialize state */
    s->cwnd = MTU;
    s->cwnd_prev = MTU;
    s->acked_bytes = 0;
    s->bytes_ignored = 0;
    s->bytes_to_ignore = 0;
    s->trigger_qa = false;
    s->qa_deadline = 0;
    s->wtd_avg = 0;
    s->fast_count = 0;
    s->fast_active = false;
    s->retrans_cnt = 0;
    s->n_pkts_acked = 0;
    s->last_decrease_time = 0;
    s->base_rtt = 12;         /* example base RTT in us */
    s->last_rtt = s->base_rtt;
    s->last_delay = 0;
    s->now = 0;
    return (cc_state_t *)s;
}

static void
smartt_cleanup(cc_state_t *st)
{
    free(st);
}

static bool can_decrease(smartt_state_t *s, int is_ecn)
{
    /* EMA for fraction of ECN-marked acks */
    s->wtd_avg = WTD_ALPHA * is_ecn + (1 - WTD_ALPHA) * s->wtd_avg;
    /* allow decrease only if fraction exceeds threshold */
    return (s->wtd_avg >= WTD_THRESH);
}

static bool quick_adapt(smartt_state_t *s)
{
    bool adapted = false;
    if (s->now >= s->qa_deadline) {
        if (s->trigger_qa && s->qa_deadline) {
            s->trigger_qa = false;
            adapted = true;
            s->cwnd = fmax(s->acked_bytes, MTU) * QA_SCALING;
            s->bytes_to_ignore = s->cwnd;
            s->bytes_ignored = 0;
        }
        s->qa_deadline = s->now + (uint64_t)(TRTT_FACTOR * s->base_rtt);
        s->acked_bytes = 0;
    }
    return adapted;
}

static bool fast_increase(smartt_state_t *s)
{
    /* FastIncrease if RTT ≈ base RTT and no ECN */
    if (fabs((double)s->last_rtt - s->base_rtt) < 1e-6 && !cc_last_pkt_ecn()) {
        s->fast_count += cc_last_pkt_size();
        if (s->fast_count > s->cwnd || s->fast_active) {
            s->cwnd += K_CONST * MTU;
            s->fast_active = true;
        }
    } else {
        s->fast_count = 0;
        s->fast_active = false;
    }
    return s->fast_active;
}

static void
smartt_on_event(cc_state_t *st, enum cc_event_t evt)
{
    smartt_state_t *s = (smartt_state_t *)st;
    s->now = cc_now();
    s->cwnd_prev = s->cwnd;

    switch (evt) {
    case CC_EVT_ACK: {
        s->retrans_cnt = 0;
        s->acked_bytes += cc_last_pkt_size();

        /* ignore Acks during QA ignore period */
        if (s->bytes_ignored < s->bytes_to_ignore) {
            s->bytes_ignored += cc_last_pkt_size();
            break;
        }

        /* recalculate target RTT */
        double trtt = TRTT_FACTOR * s->base_rtt;
        bool can_dec = can_decrease(s, cc_last_pkt_ecn());
        
        if (quick_adapt(s) || fast_increase(s))
            break;

        if (cc_last_pkt_ecn() && s->last_rtt <= trtt && can_dec) {
            /* Fair Decrease */
            s->cwnd -= (s->cwnd / BDP) * FD_CONST * cc_last_pkt_size();
        } else if (cc_last_pkt_ecn() && s->last_rtt > trtt && can_dec) {
            /* Multiplicative Decrease */
            double mdf = (s->last_rtt - trtt) / s->last_rtt * MD_CONST;
            s->cwnd -= fmin(cc_last_pkt_size(), mdf * cc_last_pkt_size());
        } else if (!cc_last_pkt_ecn() && s->last_rtt > trtt) {
            /* Fair Increase */
            s->cwnd += (cc_last_pkt_size() / s->cwnd) * MTU * FI_CONST;
        } else if (!cc_last_pkt_ecn() && s->last_rtt <= trtt) {
            /* Multiplicative Increase */
            double mi = MI_CONST(s->base_rtt, trtt);
            double inc = ((trtt - s->last_rtt) / s->last_rtt) * cc_last_pkt_size() / s->cwnd * MTU * mi;
            s->cwnd += fmin(inc, cc_last_pkt_size());
        }
        break;
    }

    case CC_EVT_NACK:
    case CC_EVT_TRIMMED:
    case CC_EVT_TIMEOUT:
        s->cwnd -= cc_last_pkt_size();
        s->trigger_qa = true;
        if (s->bytes_ignored >= s->bytes_to_ignore) {
            quick_adapt(s);
        }
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
