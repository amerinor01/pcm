#include "swift.h"

static inline pcm_float swift_target_delay(struct swift_state_shapshot *state,
                                           pcm_uint cwnd) {
    pcm_uint fs_delay =
        state->consts.fs_alpha / sqrtf((pcm_float)cwnd / state->consts.mss) +
        state->consts.fs_beta;

    if (fs_delay > state->consts.fs_range)
        fs_delay = state->consts.fs_range;

    if (fs_delay < 0.0)
        fs_delay = 0.0;

    if (cwnd == 0)
        fs_delay = 0.0;

    pcm_uint hop_delay = state->consts.hop_count * state->consts.h;
    return state->consts.brtt + fs_delay + hop_delay;
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
        state->cwnd = (pcm_float)state->cwnd * (1.0 - state->consts.max_mdf);
    }
}

static inline void swift_timeout_recovery(struct swift_state_shapshot *state) {
    state->retransmit_cnt++;
    if (state->retransmit_cnt >= state->consts.rtx_thresh) {
        state->cwnd = state->consts.mss; // min_cwnd
    } else if (state->can_decrease) {
        state->cwnd = (pcm_float)state->cwnd * (1.0 - state->consts.max_mdf);
    }
}

static inline void swift_ack_reaction(struct swift_state_shapshot *state) {
    state->retransmit_cnt = 0;
    pcm_float target_delay = swift_target_delay(state, state->cwnd);

    if (state->delay < target_delay) {
        /* Additive Increase */
        state->cwnd += state->consts.mss * state->consts.ai *
                       (state->tot_acked / state->cwnd);
        // Note: we DO NOT support FP fractional cwnd (yet)
        // if (state->cwnd >= 1) {
        //    state->cwnd += state->consts.ai * state->tot_acked / state->cwnd;
        //} else {
        //    state->cwnd += state->consts.ai * state->tot_acked;
        //}
    } else {
        /* Multiplicative Decrease */
        if (state->can_decrease) {
            pcm_float mdf =
                state->consts.beta * ((pcm_float)(state->delay - target_delay) /
                                      (pcm_float)state->delay);
            state->cwnd = (pcm_float)state->cwnd *
                          MAX(1.0 - mdf, 1.0 - state->consts.max_mdf);
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

    state.consts.brtt = 5058000;
    state.consts.bdp = 252300;
    state.consts.hop_count = 6;
    state.consts.mss = 2048;
    state.consts.rtx_thresh = 4;
    state.consts.h = (pcm_float)state.consts.brtt / 6.55;
    state.consts.fs_range = 5 * state.consts.brtt;
    state.consts.rtx_thresh = 5;
    state.consts.max_mdf = 0.5;
    state.consts.fs_alpha =
        state.consts.fs_range /
        ((1.0 / sqrt(0.1) - (1.0 / sqrt(state.consts.bdp / state.consts.mss))));
    state.consts.fs_beta =
        -(state.consts.fs_alpha / sqrt(state.consts.bdp / state.consts.mss));
    state.consts.beta = 0.8;
    state.consts.ai = 1.0;

    state.num_nacks = get_signal(SWIFT_SIG_IDX_NACK);
    state.num_rtos = get_signal(SWIFT_SIG_IDX_RTO);
    state.num_acks = get_signal(SWIFT_SIG_IDX_ACK);
    state.tot_acked = get_local_state(SWIFT_LOCAL_STATE_IDX_ACKED) +
                      state.num_acks * state.consts.mss;
    state.delay = get_signal(SWIFT_SIG_IDX_RTT);
    state.now = get_signal(SIWFT_SIG_IDX_ELAPSED_TIME);
    state.t_last_decrease =
        get_local_state(SWIFT_LOCAL_STATE_IDX_T_LAST_DECREASE);
    state.retransmit_cnt =
        get_local_state(SWIFT_LOCAL_STATE_IDX_RETRANSMIT_CNT);
    state.rtt_estim = get_local_state(SWIFT_LOCAL_STATE_IDX_RTT_ESTIM);
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
        update_signal(SWIFT_SIG_IDX_NACK, -1);
    } else if (state.num_rtos > 0) {
        swift_timeout_recovery(&state);
        update_signal(SWIFT_SIG_IDX_RTO, -1);
    } else if (state.tot_acked > 0) {
        swift_ack_reaction(&state);
    } else {
        return ERROR;
    }

    /* Enforce bounds */
    // if (state.cwnd < state->consts.mss)
    //     state.cwnd = state->consts.mss;
    // if (state.cwnd > FABRIC_MAX_CWND)
    //     state.cwnd = FABRIC_MAX_CWND;

    /*
     * update last‐decrease timestamp if we actually shrank
     * and reset RTT measurement
     */
    if (state.cwnd < state.cwnd_prev) {
        state.t_last_decrease = state.now;
        state.tot_acked = 0;
    }

    // Pacer delay is not supported (yet)
    // if (state.cwnd >= 1.0) {
    //    state.pacer_delay = state.rtt / state.cwnd;
    //} else {
    //    state.pacer_delay = 0;
    //}

    update_signal(SWIFT_SIG_IDX_ACK, -state.num_acks);
    set_local_state(SWIFT_LOCAL_STATE_IDX_ACKED, state.tot_acked);
    set_local_state(SWIFT_LOCAL_STATE_IDX_T_LAST_DECREASE,
                    state.t_last_decrease);
    set_local_state(SWIFT_LOCAL_STATE_IDX_RETRANSMIT_CNT, state.retransmit_cnt);
    set_local_state(SWIFT_LOCAL_STATE_IDX_RTT_ESTIM, state.rtt_estim);
    set_control(SWIFT_CTRL_IDX_CWND, state.cwnd);

    return SUCCESS;
}