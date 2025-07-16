#include <math.h>

#include "tcp.h"

#define TCP_NEW_RENO_SSTHRESH(state) (TCP_RTO_RECOVERY_SSTHRESH(state))
FAST_RECOVERY_DEFINE(tcp, TCP_NEW_RENO_SSTHRESH);

/**
 * @brief Naive implementation of TCP Reno-like congestion window control.
 */
int algorithm_main() {
    struct tcp_state_snapshot state = {};

    state.consts.mss = get_constant_uint(TCP_CONST_MSS);

    state.cwnd = get_control(TCP_CTRL_IDX_CWND) / state.consts.mss;

    state.num_nacks = get_signal(TCP_SIG_IDX_NACK);
    state.num_rtos = get_signal(TCP_SIG_IDX_RTO);
    state.num_acks_consumed = state.num_acks = get_signal(TCP_SIG_IDX_ACK);

    state.ssthresh = get_local_state(TCP_LOCAL_STATE_IDX_SSTHRESH);
    state.tot_acked = get_local_state(TCP_LOCAL_STATE_IDX_ACKED);
    state.in_fast_recovery = get_local_state(TCP_LOCAL_STATE_IDX_IN_FAST_RECOV);

    if (state.num_nacks > 0) {
        tcp_fast_recovery(&state);
        update_signal(TCP_SIG_IDX_NACK, -1);
        // update_signal(TCP_SIG_IDX_NACK, -state.num_nacks);
        goto exit_handler;
    }

    if (state.num_rtos > 0) {
        tcp_timeout_recovery(&state);
        update_signal(TCP_SIG_IDX_RTO, -1);
        // update_signal(TCP_SIG_IDX_RTO, -state.num_rtos);
        goto exit_handler;
    }

    if (state.in_fast_recovery && state.num_acks > 0)
        tcp_fast_recovery_exit(&state);

    if (!state.in_fast_recovery && state.num_acks > 0) {
        if (state.cwnd < state.ssthresh) {
            tcp_slow_start(&state);
        }
        if (state.num_acks) {
            tcp_cong_avoid(&state);
        }
    }

    state.num_acks_consumed -= state.num_acks;
    update_signal(TCP_SIG_IDX_ACK, -state.num_acks_consumed);

exit_handler:
    // printf("NewReno: nacks=%d rtos=%d ssthresh=%d fr=%d cwnd=%d\n",
    // state.num_nacks,
    //        state.num_rtos, state.ssthresh, state.in_fast_recovery,
    //        state.cwnd);
    set_control(TCP_CTRL_IDX_CWND, state.cwnd * state.consts.mss);
    set_local_state(TCP_LOCAL_STATE_IDX_SSTHRESH, state.ssthresh);
    set_local_state(TCP_LOCAL_STATE_IDX_ACKED, state.tot_acked);
    set_local_state(TCP_LOCAL_STATE_IDX_IN_FAST_RECOV, state.in_fast_recovery);
    return PCM_SUCCESS;
}