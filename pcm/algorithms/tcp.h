#ifndef _RENO_H_
#define _RENO_H_

#include "algo_utils.h"
#include "fabric_params.h"
#include "limits.h"
#include "pcm.h"
#include "pcm_network.h"

#define TCP_SSTHRESH_INIT INT_MAX
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

struct tcp_state_snapshot {
    pcm_uint num_nacks;
    pcm_uint num_rtos;
    pcm_uint num_acks;
    pcm_uint num_acks_consumed;
    pcm_uint cwnd;
    pcm_uint ssthresh;
    pcm_uint tot_acked;
    pcm_uint in_fast_recovery;
#ifdef BUILD_ALGO_DCTCP
    pcm_uint num_ecn;
    pcm_uint delivered;
    pcm_uint delivered_ecn;
    pcm_uint alpha;
#endif
};

#ifdef HANDLER_BUILD
int algorithm_main();

#define TCP_RTO_RECOVERY_SSTHRESH(state) (MAX((state->cwnd) >> 1, 2))

/**
 * @brief Fast Recovery
 *
 * Upon entering set cwnd=ssthresh+3 and inflate cwnd by 1 MSS per extra
 * dup‐ACK.
 */
#define FAST_RECOVERY_DEFINE(algo_name, ssthresh_comp)                         \
    static inline void algo_name##_fast_recovery(                              \
        struct tcp_state_snapshot *state) {                                    \
        if (!state->in_fast_recovery) {                                        \
            state->ssthresh = (pcm_uint)ssthresh_comp(state);                  \
            state->cwnd = state->ssthresh + 3;                                 \
            state->in_fast_recovery = 1;                                       \
        }                                                                      \
    }

/*
// In the classical NewReno, NACK data is part of SACK, therefore, each SACK still ACKs
// some data, in our current htsim setup, NACKs == trimmed packets, therefore
// we can only decrease window upon entering FR during the first NACK
// and then wait for the first ACK to exit FR
#define FAST_RECOVERY_DEFINE(algo_name, ssthresh_comp)                         \
    static inline void algo_name##_fast_recovery(                              \
        struct tcp_state_snapshot *state) {                                    \
        if (!state->in_fast_recovery) {                                        \
            state->ssthresh = (pcm_uint)ssthresh_comp(state);                  \
            state->cwnd = state->ssthresh + 3;                                 \
            state->in_fast_recovery = 1;                                       \
        } else {                                                               \
            state->cwnd += state->num_nacks;                                   \
        }                                                                      \
    }
*/

/**
 * @brief Exit from Fast Recovery
 *
 * When in fast recovery, the first non‐duplicate ACK that acknowledges
 * new data (num_acks > 0) indicates recovery is complete.  At that point:
 *   - set cwnd = ssthresh
 *   - clear fast‐recovery flag
 *   - do NOT count this ACK toward standard increase
 */
static inline void tcp_fast_recovery_exit(struct tcp_state_snapshot *state) {
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
static inline void tcp_timeout_recovery(struct tcp_state_snapshot *state) {
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
static inline void tcp_slow_start(struct tcp_state_snapshot *state) {
    pcm_uint to_ssthresh = MIN(state->num_acks, state->ssthresh - state->cwnd);
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
static inline void tcp_cong_avoid(struct tcp_state_snapshot *state) {
    if (state->tot_acked >= state->cwnd) {
        /* If credits accumulated at a higher w, apply them gently now. */
        state->tot_acked = 0;
        state->cwnd += 1;
    }
    state->tot_acked += state->num_acks;
    if (state->tot_acked >= state->cwnd) {
        pcm_uint delta = state->tot_acked / state->cwnd;
        state->tot_acked -= delta * state->cwnd;
        state->cwnd += delta;
    }
    state->num_acks = 0;
}

#else

#ifdef __cplusplus
extern "C" {
#endif

int tcp_pcmc_init(handle_t new_handle);
int dctcp_pcmc_init(handle_t new_handle);

#ifdef __cplusplus
}
#endif

#endif

#endif /* _RENO_H_ */