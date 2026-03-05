#include "newreno_v2.h"
#include "tcp_utils.h"
#include <assert.h>

#define TCP_NEW_RENO_SSTHRESH(cur_cwnd) (TCP_RTO_RECOVERY_SSTHRESH(cur_cwnd))
FAST_RECOVERY_DEFINE(tcp, TCP_NEW_RENO_SSTHRESH);

int algorithm_main() {
    pcm_uint cur_cwnd = get_control(CTRL_CWND) / MSS;

    // Calculate deltas
    pcm_uint num_acks = get_signal(SIG_ACK) - get_var_uint(VAR_PREV_ACK);
    pcm_uint num_nacks = get_signal(SIG_NACK) - get_var_uint(VAR_PREV_NACK);
    pcm_uint num_rtos = get_signal(SIG_RTO) - get_var_uint(VAR_PREV_RTO);

    // Update state cache
    set_var_uint(VAR_PREV_ACK, get_signal(SIG_ACK));
    set_var_uint(VAR_PREV_NACK, get_signal(SIG_NACK));
    set_var_uint(VAR_PREV_RTO, get_signal(SIG_RTO));

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
    if (get_var(VAR_IN_FAST_RECOV) && acks_to_consume > 0)
        tcp_fast_recovery_exit(ALGO_CTX_PASS, &cur_cwnd, &acks_to_consume);

    if (!get_var(VAR_IN_FAST_RECOV) && acks_to_consume > 0) {
        if (cur_cwnd < get_var(VAR_SSTHRESH)) {
            tcp_slow_start(ALGO_CTX_PASS, &cur_cwnd, &acks_to_consume);
        }
        if (acks_to_consume) {
            tcp_cong_avoid(ALGO_CTX_PASS, &cur_cwnd, &acks_to_consume);
        }
    }

adjust_cwnd:
    set_control(CTRL_CWND, cur_cwnd * MSS);

    return PCM_SUCCESS;
}
