#include <assert.h>

#include "newreno.h"
#include "fabric_params.h"
#include "tcp_utils.h"

#define TCP_NEW_RENO_SSTHRESH(cur_cwnd) (TCP_RTO_RECOVERY_SSTHRESH(cur_cwnd))
FAST_RECOVERY_DEFINE(tcp, TCP_NEW_RENO_SSTHRESH);

/**
 * @brief Naive implementation of TCP Reno-like congestion window control.
 */
int algorithm_main() {
    pcm_uint cur_cwnd = get_control(CTRL_CWND) / FABRIC_LINK_MSS;
    pcm_uint num_acks = get_signal(SIG_ACK);

    /*
     * Negative feedback part has higher priority
     */
    pcm_uint acks_to_consume = 0;

    if (get_signal(SIG_NACK) > 0) {
        tcp_fast_recovery(ALGO_CTX_PASS, &cur_cwnd);
        update_signal(SIG_NACK, -1);
        goto adjust_cwnd;
    }

    if (get_signal(SIG_RTO) > 0) {
        tcp_timeout_recovery(ALGO_CTX_PASS, &cur_cwnd);
        update_signal(SIG_RTO, -1);
        goto adjust_cwnd;
    }

    /*
     * We have no positive feedback and at least one ACK (otherwise we wouldn't
     * be triggered)
     */
    assert(get_signal(SIG_ACK));
    acks_to_consume = num_acks;
    if (get_local_state(VAR_IN_FAST_RECOV) && acks_to_consume > 0)
        tcp_fast_recovery_exit(ALGO_CTX_PASS, &cur_cwnd, &acks_to_consume);

    /*
     * Fallback to rate increase
     */
    if (!get_local_state(VAR_IN_FAST_RECOV) && acks_to_consume > 0) {
        if (cur_cwnd < get_local_state(VAR_SSTHRESH)) {
            tcp_slow_start(ALGO_CTX_PASS, &cur_cwnd, &acks_to_consume);
        }
        if (acks_to_consume) {
            tcp_cong_avoid(ALGO_CTX_PASS, &cur_cwnd, &acks_to_consume);
        }
    }

adjust_cwnd:
    update_signal(SIG_ACK, -(num_acks - acks_to_consume));
    set_control(CTRL_CWND, cur_cwnd * FABRIC_LINK_MSS);

    return PCM_SUCCESS;
}