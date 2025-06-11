#ifndef _RENO_H_
#define _RENO_H_

#include "pcm.h"

#define MAX(a, b)                                                              \
    ({                                                                         \
        __typeof__(a) _a = (a);                                                \
        __typeof__(b) _b = (b);                                                \
        _a > _b ? _a : _b;                                                     \
    })

#define MIN(a, b)                                                              \
    ({                                                                         \
        __typeof__(a) _a = (a);                                                \
        __typeof__(b) _b = (b);                                                \
        _a < _b ? _a : _b;                                                     \
    })

#define MIN_NOT_ZERO(x, y)                                                     \
    ({                                                                         \
        typeof(x) __x = (x);                                                   \
        typeof(y) __y = (y);                                                   \
        __x == 0 ? __y : ((__y == 0) ? __x : MIN(__x, __y));                   \
    })

#define DCTCP_MAX_ALPHA 1024U
#define DCTCP_SHIFT_G 4 /* g = 1/2^4 EWMA weight */

enum tcp_signal_idxs {
    TCP_SIG_IDX_NACK = 0,
    TCP_SIG_IDX_RTO = 1,
    TCP_SIG_IDX_ACK = 2,
    TCP_SIG_IDX_ECN = 3
};

enum tcp_control_idxs { TCP_CTRL_IDX_CWND = 0 };

enum tcp_local_var_idxs {
    TCP_LOCAL_STATE_IDX_SSTHRESH = 0,
    TCP_LOCAL_STATE_IDX_ACKED = 1,
    TCP_LOCAL_STATE_IDX_IN_FAST_RECOV = 2,
    TCP_LOCAL_STATE_IDX_EPOCH_DELIVERED = 3,
    TCP_LOCAL_STATE_IDX_EPOCH_ECN_DELIVERED = 4,
    TCP_LOCAL_STATE_IDX_ALPHA = 5
};

#ifdef HANDLER_BUILD
int algorithm_main();

#define TCP_RTO_RECOVERY_SSTHRESH(state) (MAX((state->cwnd) >> 1, 2))

struct tcp_state_snapshot {
    int num_nacks;
    int num_rtos;
    int num_acks;
    int cwnd;
    int ssthresh;
    int tot_acked;
    int in_fast_recovery;
#ifdef BUILD_ALGO_DCTCP
    uint32_t num_ecn;
    uint32_t delivered;
    uint32_t delivered_ecn;
    uint32_t alpha;
#endif
};

/**
 * @brief Fast Recovery
 *
 * Upon entering set cwnd=ssthresh+3 and inflate cwnd by 1 MSS per extra
 * dup‐ACK
 */
#define FAST_RECOVERY_DEFINE(algo_name, ssthresh_comp)                         \
    inline void algo_name##_fast_recovery(struct tcp_state_snapshot *state) {  \
        if (!state->in_fast_recovery) {                                        \
            state->ssthresh = (int)ssthresh_comp(state);                       \
            state->cwnd = state->ssthresh + 3;                                 \
            state->in_fast_recovery = 1;                                       \
        } else {                                                               \
            state->cwnd += state->num_nacks;                                   \
        }                                                                      \
    }

/**
 * @brief Exit from Fast Recovery
 *
 * When in fast recovery, the first non‐duplicate ACK that acknowledges
 * new data (num_acks > 0) indicates recovery is complete.  At that point:
 *   - set cwnd = ssthresh
 *   - clear fast‐recovery flag
 *   - do NOT count this ACK toward standard increase
 */
inline void tcp_fast_recovery_exit(struct tcp_state_snapshot *state) {
    state->cwnd = state->ssthresh;
    state->in_fast_recovery = 0;
    state->num_acks -= 1;
}

/**
 * @brief Timeout Recovery (RTO)
 *
 * On any RTO, unconditionally halve cwnd -> ssthresh, set cwnd = 1 MSS,
 * and exit fast recovery if we were in it.
 */
inline void tcp_timeout_recovery(struct tcp_state_snapshot *state) {
    state->ssthresh = TCP_RTO_RECOVERY_SSTHRESH(state);
    state->cwnd = 1;
    state->in_fast_recovery = 0;
}

/**
 * @brief Slow Start
 * https://github.com/torvalds/linux/blob/aef17cb3d3c43854002956f24c24ec8e1a0e3546/net/ipv4/tcp_cong.c#L455
 *
 * - In slow start (cwnd < ssthresh), cwnd += 1 MSS per ACK.
 */
inline void tcp_slow_start(struct tcp_state_snapshot *state) {
    int to_ssthresh = MIN(state->num_acks, state->ssthresh - state->cwnd);
    state->cwnd += to_ssthresh;
    state->num_acks -= to_ssthresh;
}

/**
 * @brief Congestion Avoidance (cwnd >= ssthresh)
 * https://github.com/torvalds/linux/blob/aef17cb3d3c43854002956f24c24ec8e1a0e3546/net/ipv4/tcp_cong.c#L469
 *
 * - increment by ~1 MSS per RTT: accumulate tot_acked += num_acks;
 * - when tot_acked >= cwnd, do cwnd++,
 * - subtract cwnd from tot_acked to preserve any “leftover” credit.
 */
inline void tcp_cong_avoid(struct tcp_state_snapshot *state) {

    if (state->tot_acked >= state->cwnd) {
        /* If credits accumulated at a higher w, apply them gently now. */
        state->tot_acked = 0;
        state->cwnd += 1;
    }
    state->tot_acked += state->num_acks;
    if (state->tot_acked >= state->cwnd) {
        int delta = state->tot_acked / state->cwnd;
        state->tot_acked -= delta * state->cwnd;
        state->cwnd += delta;
    }
    state->num_acks = 0;
}

#endif

#endif /* _RENO_H_ */