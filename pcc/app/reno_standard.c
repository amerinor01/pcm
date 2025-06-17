#include <math.h>

#include "tcp.h"

#define TCP_NEW_RENO_SSTHRESH(state) (TCP_RTO_RECOVERY_SSTHRESH(state))
FAST_RECOVERY_DEFINE(tcp, TCP_NEW_RENO_SSTHRESH);

/**
 * @brief Naive implementation of TCP Reno-like congestion window control.
 */
int algorithm_main() {
    struct tcp_state_snapshot state = {};
    state.num_nacks = get_signal(TCP_SIG_IDX_NACK);
    state.num_rtos = get_signal(TCP_SIG_IDX_RTO);
    state.num_acks = get_signal(TCP_SIG_IDX_ACK);
    state.cwnd = get_control(TCP_CTRL_IDX_CWND) / FABRIC_LINK_MTU;
    state.ssthresh = get_local_state(TCP_LOCAL_STATE_IDX_SSTHRESH);
    state.tot_acked = get_local_state(TCP_LOCAL_STATE_IDX_ACKED);
    state.in_fast_recovery = get_local_state(TCP_LOCAL_STATE_IDX_IN_FAST_RECOV);

    if (state.num_nacks > 0) {
        tcp_fast_recovery(&state);
        set_signal(TCP_SIG_IDX_NACK, 0);
        goto exit_handler;
    }

    if (state.num_rtos > 0) {
        tcp_timeout_recovery(&state);
        set_signal(TCP_SIG_IDX_RTO, 0);
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

exit_handler:
    set_signal(TCP_SIG_IDX_ACK, state.num_acks);
    set_control(TCP_CTRL_IDX_CWND, state.cwnd * FABRIC_LINK_MTU);
    set_local_state(TCP_LOCAL_STATE_IDX_SSTHRESH, state.ssthresh);
    set_local_state(TCP_LOCAL_STATE_IDX_ACKED, state.tot_acked);
    set_local_state(TCP_LOCAL_STATE_IDX_IN_FAST_RECOV, state.in_fast_recovery);
    return SUCCESS;
}