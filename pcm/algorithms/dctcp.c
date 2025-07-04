#include <assert.h>
#include <math.h>

#include "tcp.h"

#define DCTCP_SSTHRESH(state)                                                  \
    (MAX((uint32_t)state->cwnd -                                               \
             (((uint32_t)state->cwnd * state->alpha) >> 11U),                  \
         2U))
FAST_RECOVERY_DEFINE(dctcp, DCTCP_SSTHRESH);

int algorithm_main() {
    struct tcp_state_snapshot state = {0};
    state.num_nacks = get_signal(TCP_SIG_IDX_NACK);
    state.num_rtos = get_signal(TCP_SIG_IDX_RTO);
    state.num_acks_consumed = state.num_acks = get_signal(TCP_SIG_IDX_ACK);
    state.num_ecn = (uint32_t)get_signal(TCP_SIG_IDX_ECN);
    state.cwnd = get_control(TCP_CTRL_IDX_CWND) / FABRIC_LINK_MTU;
    state.ssthresh = get_local_state(TCP_LOCAL_STATE_IDX_SSTHRESH);
    state.tot_acked = get_local_state(TCP_LOCAL_STATE_IDX_ACKED);
    state.in_fast_recovery = get_local_state(TCP_LOCAL_STATE_IDX_IN_FAST_RECOV);
    state.delivered =
        (uint32_t)get_local_state(TCP_LOCAL_STATE_IDX_EPOCH_DELIVERED);
    state.delivered_ecn =
        (uint32_t)get_local_state(TCP_LOCAL_STATE_IDX_EPOCH_ECN_DELIVERED);
    state.alpha = get_local_state(TCP_LOCAL_STATE_IDX_ALPHA);

    if (state.num_nacks > 0) {
        dctcp_fast_recovery(&state);
        update_signal(TCP_SIG_IDX_NACK, -state.num_nacks);
        goto exit_handler;
    }

    if (state.num_rtos > 0) {
        tcp_timeout_recovery(&state);
        update_signal(TCP_SIG_IDX_RTO, -state.num_rtos);
        goto exit_handler;
    }

    /*
     * Execute DCTCP alpha update logic here before state.num_acks got consumed
     * in ACK processing.
     * */
    state.delivered += state.num_acks;
    state.delivered_ecn += state.num_ecn;

    /*
     * Expired RTT
     *
     * Note: Linux DCTCP detects it based on reaching seq number that at the
     * window boundary. We do this simpler in expectation that receiving cwnd of
     * ACKs takes RTT.
     *
     * https://github.com/torvalds/linux/blob/aef17cb3d3c43854002956f24c24ec8e1a0e3546/net/ipv4/tcp_dctcp.c#L132
     */
    if (state.delivered >= state.cwnd) {
        /* TODO: support LB path change */
        /* alpha = (1 - g) * alpha + g * F */
        state.alpha -= MIN_NOT_ZERO(state.alpha, state.alpha >> DCTCP_SHIFT_G);
        if (state.delivered_ecn) {
            state.delivered_ecn <<= (10 - DCTCP_SHIFT_G);
            state.delivered_ecn /= MAX(1U, state.delivered);

            state.alpha =
                MIN(state.alpha + state.delivered_ecn, DCTCP_MAX_ALPHA);
        }
        set_local_state(TCP_LOCAL_STATE_IDX_ALPHA, state.alpha);
        set_local_state(TCP_LOCAL_STATE_IDX_EPOCH_DELIVERED, 0);
        set_local_state(TCP_LOCAL_STATE_IDX_EPOCH_ECN_DELIVERED, 0);
        update_signal(TCP_SIG_IDX_ECN, -state.num_ecn);
    }

    if (state.in_fast_recovery && state.num_acks > 0)
        tcp_fast_recovery_exit(&state);

    if (!state.in_fast_recovery && state.num_acks > 0) {
        assert(get_signal(TCP_SIG_IDX_ACK) ==
               ((pcm_uint *)signals)[TCP_SIG_IDX_ACK]);
        printf("TEST OF POINTER: YO YO YO: %d %d\n",
               (int) get_signal(TCP_SIG_IDX_ACK),
               (int) ((pcm_uint *)signals)[TCP_SIG_IDX_ACK]);
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
    set_control(TCP_CTRL_IDX_CWND, state.cwnd * FABRIC_LINK_MTU);
    set_local_state(TCP_LOCAL_STATE_IDX_SSTHRESH, state.ssthresh);
    set_local_state(TCP_LOCAL_STATE_IDX_ACKED, state.tot_acked);
    set_local_state(TCP_LOCAL_STATE_IDX_IN_FAST_RECOV, state.in_fast_recovery);

    return SUCCESS;
}