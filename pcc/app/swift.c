#include "swift.h"

static inline float swift_target_delay(int cwnd) {
    int fs_delay =
        SWIFT_FS_ALPHA / sqrtf((float)cwnd / SWIFT_MSS) + SWIFT_FS_BETA;

    if (fs_delay > SWIFT_FS_RANGE)
        fs_delay = SWIFT_FS_RANGE;

    if (fs_delay < 0.0)
        fs_delay = 0.0;

    if (cwnd == 0)
        fs_delay = 0.0;

    int hop_delay = SWIFT_HOP_COUNT * SWIFT_H;
    return SWIFT_BASE_DELAY + fs_delay + hop_delay;
}

static inline void swift_rtt_estimate(struct swift_state_shapshot *state) {
    if (state->rtt_estim > 0) {
        state->rtt_estim = 7 * state->rtt_estim / 8 + state->delay / 8;
    } else {
        state->rtt_estim = state->delay;
    }
}

static inline void swift_nack_recovery(struct swift_state_shapshot *state) {
    state->retransmit_cnt = 0;
    if (state->can_decrease) {
        state->cwnd = (float)state->cwnd * (1.0 - SWIFT_MAX_MDF);
    }
}

static inline void swift_timeout_recovery(struct swift_state_shapshot *state) {
    state->retransmit_cnt++;
    if (state->retransmit_cnt >= RETX_RESET_THRESHOLD) {
        state->cwnd = SWIFT_MIN_CWND;
    } else if (state->can_decrease) {
        state->cwnd = (float)state->cwnd * (1.0 - SWIFT_MAX_MDF);
    }
}

static inline void swift_ack_reaction(struct swift_state_shapshot *state) {
    state->retransmit_cnt = 0;
    float target_delay = swift_target_delay(state->cwnd);

    if (state->delay < target_delay) {
        /* Additive Increase */
        state->cwnd += SWIFT_AI * state->num_acks / state->cwnd;
        // Note: we DO NOT support FP fractional cwnd (yet)
        // if (state->cwnd >= 1) {
        //    state->cwnd += SWIFT_AI * state->num_acks / state->cwnd;
        //} else {
        //    state->cwnd += SWIFT_AI * state->num_acks;
        //}
    } else {
        /* Multiplicative Decrease */
        if (state->can_decrease) {
            float mdf = SWIFT_BETA * ((float)(state->delay - target_delay) /
                                      (float)state->delay);
            state->cwnd =
                (float)state->cwnd * MAX(1.0 - mdf, 1.0 - SWIFT_MAX_MDF);
        }
    }
}

/*
 * Swift CC loop without support for fast recovery detection and
 * pacer delay and FP cwnd
 * Based on the htsim implementation of Swift:
 *
 * https://github.com/Broadcom/csg-htsim/blob/67cbbbb1cccdbddc400803414127d044ecd55290/sim/swift_new.cpp#L71
 *
 */
int algorithm_main() {
    struct swift_state_shapshot state;
    state.num_nacks = get_signal(SWIFT_SIG_IDX_NACK);
    state.num_rtos = get_signal(SWIFT_SIG_IDX_RTO);
    state.num_acks = get_local_state(SWIFT_LOCAL_STATE_IDX_ACKED) +
                     get_signal(SWIFT_SIG_IDX_ACK);
    state.delay = get_signal(SWIFT_SIG_IDX_RTT);
    state.now = get_signal(SIWFT_SIG_IDX_ELAPSED_TIME);
    state.t_last_decrease =
        get_local_state(SWIFT_LOCAL_STATE_IDX_T_LAST_DECREASE);
    state.retransmit_cnt =
        get_local_state(SWIFT_LOCAL_STATE_IDX_RETRANSMIT_CNT);
    state.cwnd = get_control(SWIFT_CTRL_IDX_CWND);

    state.can_decrease = (state.now - state.t_last_decrease) >= state.rtt_estim;
    state.cwnd_prev = state.cwnd;

    /*
     * Note: for some reason htsim computes RTT estimation after computing
     * can_decrease predicate.
     */
    swift_rtt_estimate(&state);

    if (state.num_nacks > 0) {
        swift_nack_recovery(&state);
        set_signal(SWIFT_SIG_IDX_NACK, 0);
        goto exit_handler;
    } else if (state.num_rtos > 0) {
        swift_timeout_recovery(&state);
        set_signal(SWIFT_SIG_IDX_RTO, 0);
        goto exit_handler;
    } else if (state.num_acks > 0) {
        swift_ack_reaction(&state);
    } else {
        return ERROR;
    }

    /* Enforce bounds */
    if (state.cwnd < SWIFT_MIN_CWND)
        state.cwnd = SWIFT_MIN_CWND;
    if (state.cwnd > SWIFT_MAX_CWND)
        state.cwnd = SWIFT_MAX_CWND;

    /*
     * update last‐decrease timestamp if we actually shrank
     * and reset RTT measurement
     */
    if (state.cwnd < state.cwnd_prev) {
        state.t_last_decrease = state.now;
        state.num_acks = 0;
    }

    // Pacer delay is not supported (yet)
    // if (state.cwnd >= 1.0) {
    //    state.pacer_delay = state.rtt / state.cwnd;
    //} else {
    //    state.pacer_delay = 0;
    //}

exit_handler:
    set_signal(SWIFT_SIG_IDX_ACK, 0);
    set_local_state(SWIFT_LOCAL_STATE_IDX_ACKED, state.num_acks);
    set_local_state(SWIFT_LOCAL_STATE_IDX_T_LAST_DECREASE,
                    state.t_last_decrease);
    set_local_state(SWIFT_LOCAL_STATE_IDX_RETRANSMIT_CNT, state.retransmit_cnt);
    set_control(SWIFT_CTRL_IDX_CWND, state.cwnd);

    return SUCCESS;
}