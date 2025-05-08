/* swift.c – TCP Swift sender control loop */
#include "cc_box_api.h"
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#define RETX_RESET_THRESHOLD 4    /* as in S3.6 */
#define MIN_CWND             1.0  /* in MSS units */
#define MAX_CWND           10000.0/* some implementation cap */
#define SWIFT_BASE_DELAY 20.0
#define SWIFT_H (SWIFT_BASE_DELAY / 6.55)
#define SWIFT_HOP_COUNT 5
#define SWIFT_FS_RANGE (5 * SWIFT_BASE_DELAY)
#define SWIFT_MIN_CWND 0.1  // note: in packets                                                                                                                         
#define SWIFT_MAX_CWND 100.0   // note: in packets
#define SWIFT_FS_ALPHA (SWIFT_FS_RANGE / ((1.0 / sqrt(SWIFT_MIN_CWND)) -  (1.0 / sqrt(SWIFT_MAX_CWND))))
#define SWIFT_FS_BETA - SWIFT_FS_ALPHA / sqrt(SWIFT_MAX_CWND)
#define SWIFT_MAX_MDF 0.5 // max multiplicate decrease factor.  Value is a guess   
#define SWIFT_AI 1.0  // increase constant.  Value is a guess
#define SWIFT_BETA 0.8   // decrease constant.  Value is a guess
#define SWIFT_MSS 4096

typedef struct {
    uint64_t pacer_delay;
    double cwnd;            /* congestion window (MSS units) */
    double cwnd_prev;       /* window before last decrease */
    uint32_t retransmit_cnt;
    uint32_t n_pkts_acked;
    uint64_t now;
    uint64_t t_last_decrease; /* timestamp of last MD (ns or µs) */
    uint64_t rtt; /* roundtrip time from last packet that was measured */
    uint64_t delay; /* a component of RTT */
} swift_state_t;

static cc_state_t *
swift_init(void *params)
{
    (void)params;
    swift_state_t *s = malloc(sizeof(*s));
    if (!s) return NULL;

    /* initialize parameters (tune these) */
    s->cwnd     = 1.0;
    s->cwnd_prev = 1.0;
    s->retransmit_cnt = 0;
    s->t_last_decrease = 0;
    return (cc_state_t *)s;
}

static void
swift_cleanup(cc_state_t *st)
{
    free(st);
}

double target_delay(uint32_t cwnd)
{
    double fs_delay = SWIFT_FS_ALPHA / sqrt(cwnd / SWIFT_MSS) + SWIFT_FS_BETA;

    if (fs_delay > SWIFT_FS_RANGE) {
        fs_delay = SWIFT_FS_RANGE;
    }
    if (fs_delay < 0.0) {
        fs_delay = 0.0;
    }

    if (cwnd == 0) {
        fs_delay = 0.0;
    }

    double hop_delay = SWIFT_HOP_COUNT * SWIFT_H;
    return SWIFT_BASE_DELAY + fs_delay + hop_delay;
}

static void
swift_on_event(cc_state_t *st, enum cc_event_t evt)
{
    swift_state_t *s = (swift_state_t *)st;
    uint64_t now = cc_now();      /* in µs or ns */
    int can_decrease = (now - s->t_last_decrease) >= s->rtt;
    s->cwnd_prev = s->cwnd;

    switch (evt) {
    case CC_EVT_ACK:
        /* On Receiving ACK */
        s->retransmit_cnt = 0;
        double tgt_delay = target_delay(s->cwnd);

        if (s->delay < tgt_delay) {
            /* Additive Increase */
            if (s->cwnd >= 1) {
                s->cwnd += SWIFT_AI / s->cwnd * s->n_pkts_acked;
            } else {
                s->cwnd += SWIFT_AI * s->n_pkts_acked;
            }
        } else {
            /* Multiplicative Decrease */
            if (can_decrease) {
                double mdf = SWIFT_BETA * ((s->delay - tgt_delay) / s->delay);
                s->cwnd *= max(1.0 - SWIFT_MAX_MDF, 1.0 - mdf);
            }
        }
        break;

    case CC_EVT_TIMEOUT:
        /* On Retransmit Timeout */
        s->retransmit_cnt++;
        if (s->retransmit_cnt >= RETX_RESET_THRESHOLD) {
            s->cwnd = MIN_CWND;
        } else if (can_decrease) {
            s->cwnd *= (1.0 - SWIFT_MAX_MDF);
        }
        break;

    case CC_EVT_NACK:
        /* On Fast Recovery (dup-ACKs) */
        s->retransmit_cnt = 0;
        if (can_decrease) {
            s->cwnd *= (1.0 - SWIFT_MAX_MDF);
        }
        break;

    default:
        /* no action */
        return;
    }

    /* Enforce bounds */
    if (s->cwnd < MIN_CWND) s->cwnd = MIN_CWND;
    if (s->cwnd > MAX_CWND) s->cwnd = MAX_CWND;

    /* update last‐decrease timestamp if we actually shrank */
    if (s->cwnd <= s->cwnd_prev) {
        s->t_last_decrease = s->now;
    }

    if (s->cwnd >= 1.0) {
        s->pacer_delay = s->rtt / s->cwnd;
    } else {
        s->pacer_delay = 0;
    }
    return;
}

static uint32_t
swift_get_rate(cc_state_t *st)
{
    swift_state_t *s = (swift_state_t *)st;
    /* return floor(cwnd) in MSS units */
    return (uint32_t)floor(s->cwnd);
}

const cc_algo_t swift_algo = {
    .init      = swift_init,
    .cleanup   = swift_cleanup,
    .on_event  = swift_on_event,
    .get_rate  = swift_get_rate,
    .set_param = NULL,   /* you could parse ai/β/max_mdf here */
};
