#include "fabric_params.h"
#include "newreno.h"
#include "tcp_utils.h"

#define TCP_NEW_RENO_SSTHRESH(cur_cwnd) (TCP_RTO_RECOVERY_SSTHRESH(cur_cwnd))
FAST_RECOVERY_DEFINE(tcp, TCP_NEW_RENO_SSTHRESH);

/**
 * @brief Naive implementation of TCP Reno-like congestion window control.
 */
int algorithm_main() {
    pcm_uint cur_cwnd = get_control(CTRL_CWND) / FABRIC_LINK_MSS;

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

    pcm_uint num_acks = get_signal(SIG_ACK);
    pcm_uint acks_to_consume = num_acks;

    if (get_local_state(VAR_IN_FAST_RECOV) && acks_to_consume > 0)
        tcp_fast_recovery_exit(ALGO_CTX_PASS, &cur_cwnd, &acks_to_consume);

    if (!get_local_state(VAR_IN_FAST_RECOV) && acks_to_consume > 0) {
        if (cur_cwnd < get_local_state(VAR_SSTHRESH)) {
            tcp_slow_start(ALGO_CTX_PASS, &cur_cwnd, &acks_to_consume);
        }
        if (acks_to_consume) {
            tcp_cong_avoid(ALGO_CTX_PASS, &cur_cwnd, &acks_to_consume);
        }
    }

    update_signal(SIG_ACK, -(num_acks - acks_to_consume));

adjust_cwnd:
    set_control(CTRL_CWND, cur_cwnd * FABRIC_LINK_MSS);

    return PCM_SUCCESS;
}