#ifndef _TCP_UTILS_H_
#define _TCP_UTILS_H_

#include "algo_utils.h"
#include "pcm.h"
#include "pcmh.h"

#ifdef BUILD_ALGO_NEWRENO
#include "newreno.h"
#elif BUILD_ALGO_DCTCP
#include "dctcp.h"
#elif BUILD_ALGO_NEWRENO_V2
#include "newreno_v2.h"
#elif BUILD_ALGO_DCTCP_V2
#include "dctcp_v2.h"
#endif

/* Generic TCP-related code shared by NewReno and DCTCP */

#define TCP_RTO_RECOVERY_SSTHRESH(cur_cwnd) (MAX((cur_cwnd) >> 1, 2U))

/**
 * @brief Fast Recovery (FR)
 *
 * Note: In the classical NewReno, upon entering set cwnd=ssthresh+3 and inflate
 * cwnd by 1 MSS per extra dup‐ACK. This is posible because NACK is a part of
 * SACK and each SACK still ACKs some data.
 *
 * In contrast, in our current setup (htsim/UE), NACKs
 * are coming from trimmed packets and don't carry any positive feedback.
 * Therefore we can only decrease window upon entering FR during the first NACK
 * and then wait for the first ACK to exit FR.
 */
#define FAST_RECOVERY_DEFINE(algo_name, ssthresh_comp)                         \
    static PCM_FORCE_INLINE void algo_name##_fast_recovery(                    \
        ALGO_CTX_ARGS, pcm_uint *cur_cwnd) {                                   \
        if (!get_var(VAR_IN_FAST_RECOV)) {                                     \
            pcm_uint new_ssthresh = (pcm_uint)ssthresh_comp(*cur_cwnd);        \
            *cur_cwnd = new_ssthresh + 3;                                      \
            set_var_uint(VAR_SSTHRESH, new_ssthresh);                          \
            set_var_uint(VAR_IN_FAST_RECOV, 1);                                \
        }                                                                      \
    }

/**
 * @brief Exit from Fast Recovery
 *
 * In FR, the first non‐duplicate ACK that acknowledges new data (num_acks > 0)
 * indicates recovery is complete.
 *
 * At that point:
 *   - set cwnd = ssthresh
 *   - clear fast‐recovery flag
 *   - do NOT count this ACK toward standard increase
 */
void tcp_fast_recovery_exit(ALGO_CTX_ARGS, pcm_uint *cur_cwnd,
                            pcm_uint *num_acks);

/**
 * @brief Timeout Recovery (RTO)
 *
 * On any RTO, unconditionally halve cwnd -> ssthresh, set cwnd = 1 MSS,
 * and exit fast recovery if we were in it.
 */
void tcp_timeout_recovery(ALGO_CTX_ARGS, pcm_uint *cur_cwnd);

/**
 * @brief Slow Start
 * https://github.com/torvalds/linux/blob/aef17cb3d3c43854002956f24c24ec8e1a0e3546/net/ipv4/tcp_cong.c#L455
 *
 * - In slow start (cwnd < ssthresh), cwnd += 1 MSS per ACK.
 */
void tcp_slow_start(ALGO_CTX_ARGS, pcm_uint *cur_cwnd,
                    pcm_uint *acks_to_consume);
/**
 * @brief Congestion Avoidance (cwnd >= ssthresh)
 * https://github.com/torvalds/linux/blob/aef17cb3d3c43854002956f24c24ec8e1a0e3546/net/ipv4/tcp_cong.c#L469
 *
 * - increment by ~1 MSS per RTT: accumulate tot_acked += num_acks;
 * - when tot_acked >= cwnd, do cwnd++,
 * - subtract cwnd from tot_acked to preserve any “leftover” credit.
 */
void tcp_cong_avoid(ALGO_CTX_ARGS, pcm_uint *cur_cwnd,
                    pcm_uint *acks_to_consume);

#endif /* _TCP_UTILS_H_ */
