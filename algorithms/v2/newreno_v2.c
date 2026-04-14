#include "newreno_v2.h"
#include "tcp_utils.h"
#include <assert.h>

#define TCP_NEW_RENO_SSTHRESH(cur_cwnd) (TCP_RTO_RECOVERY_SSTHRESH(cur_cwnd))
FAST_RECOVERY_DEFINE(tcp, TCP_NEW_RENO_SSTHRESH);

int algorithm_main() {
    pcm_uint cur_cwnd = get_control(CWND) / MSS;

    // Calculate deltas
    pcm_uint num_acks = get_signal(ACK) - get_var_uint(PREV_ACK);
    pcm_uint num_nacks = get_signal(NACK) - get_var_uint(PREV_NACK);
    pcm_uint num_rtos = get_signal(RTO) - get_var_uint(PREV_RTO);

    // Update state cache
    set_var_uint(PREV_ACK, get_signal(ACK));
    set_var_uint(PREV_NACK, get_signal(NACK));
    set_var_uint(PREV_RTO, get_signal(RTO));

    pcm_uint acks_to_consume = 0;

    if (num_nacks > 0) {
        tcp_fast_recovery(ALGO_CTX_PASS, &cur_cwnd);
        goto adjust_cwnd;
    }

    if (num_rtos > 0) {
        tcp_timeout_recovery(ALGO_CTX_PASS, &cur_cwnd);
        goto adjust_cwnd;
    }

    acks_to_consume = num_acks;
    if (get_var(IN_FAST_RECOV) && acks_to_consume > 0)
        tcp_fast_recovery_exit(ALGO_CTX_PASS, &cur_cwnd, &acks_to_consume);

    if (!get_var(IN_FAST_RECOV) && acks_to_consume > 0) {
        if (cur_cwnd < get_var(SSTHRESH)) {
            tcp_slow_start(ALGO_CTX_PASS, &cur_cwnd, &acks_to_consume);
        }
        if (acks_to_consume) {
            tcp_cong_avoid(ALGO_CTX_PASS, &cur_cwnd, &acks_to_consume);
        }
    }

adjust_cwnd:
    set_control(CWND, cur_cwnd * MSS);

    return PCM_SUCCESS;
}
