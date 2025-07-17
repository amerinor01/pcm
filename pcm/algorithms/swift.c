#include "swift.h"

static inline pcm_float swift_target_delay(ALGO_CTX_ARGS, pcm_uint cwnd) {
    pcm_uint fs_delay = FS_ALPHA / sqrtf((pcm_float)cwnd / MSS) + FS_BETA;

    if (fs_delay > FS_RANGE)
        fs_delay = FS_RANGE;

    if (fs_delay < 0.0)
        fs_delay = 0.0;

    if (cwnd == 0)
        fs_delay = 0.0;

    pcm_uint hop_delay = HOP_COUNT * H;
    return BRTT + fs_delay + hop_delay;
}

static inline void swift_rtt_estimate(ALGO_CTX_ARGS,
                                      struct swift_state_snapshot *state) {
    if (state->rtt_estim > 0) {
        state->rtt_estim = 7 * state->rtt_estim / 8 + state->delay / 8;
    } else {
        state->rtt_estim = state->delay;
    }
}

static inline void swift_nack_recovery(ALGO_CTX_ARGS,
                                       struct swift_state_snapshot *state) {
    state->retransmit_cnt = 0;
    if (state->can_decrease) {
        state->cwnd = (pcm_float)state->cwnd * (1.0 - MAX_MDF);
    }
}

static inline void swift_timeout_recovery(ALGO_CTX_ARGS,
                                          struct swift_state_snapshot *state) {
    state->retransmit_cnt++;
    if (state->retransmit_cnt >= RTX_RESET_THRESH) {
        state->cwnd = MSS; // min_cwnd
    } else if (state->can_decrease) {
        state->cwnd = (pcm_float)state->cwnd * (1.0 - MAX_MDF);
    }
}

static inline void swift_ack_reaction(ALGO_CTX_ARGS,
                                      struct swift_state_snapshot *state) {
    state->retransmit_cnt = 0;
    pcm_float target_delay = swift_target_delay(ALGO_CTX_PASS, state->cwnd);

    if (state->delay < target_delay) {
        /* Additive Increase */
        state->cwnd += MSS * AI * (state->tot_acked / state->cwnd);
        // Note: we DO NOT support FP fractional cwnd (yet)
        // if (state->cwnd >= 1) {
        //    state->cwnd += AI * state->tot_acked / state->cwnd;
        //} else {
        //    state->cwnd += AI * state->tot_acked;
        //}
    } else {
        /* Multiplicative Decrease */
        if (state->can_decrease) {
            pcm_float mdf = BETA * ((pcm_float)(state->delay - target_delay) /
                                    (pcm_float)state->delay);
            state->cwnd =
                (pcm_float)state->cwnd * MAX(1.0 - mdf, 1.0 - MAX_MDF);
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
    struct swift_state_snapshot state;

    state.num_nacks = get_signal(SIG_NACK);
    state.num_rtos = get_signal(SIG_RTO);
    state.num_acks = get_signal(SIG_ACK);
    state.tot_acked = get_local_state(VAR_ACKED) + state.num_acks * MSS;
    state.delay = get_signal(SIG_RTT);
    state.now = get_signal(SIG_ELAPSED_TIME);

    state.t_last_decrease = get_local_state(VAR_T_LAST_DECREASE);
    state.retransmit_cnt = get_local_state(VAR_RTX_CNT);
    state.rtt_estim = get_local_state(VAR_RTT_ESTIM);

    state.cwnd = get_control(CTRL_CWND);

    state.can_decrease = (state.now - state.t_last_decrease) >= state.rtt_estim;
    state.cwnd_prev = state.cwnd;

    /*
     * Note: for some reason htsim computes RTT estimation after computing
     * can_decrease predicate.
     */
    swift_rtt_estimate(ALGO_CTX_PASS, &state);

    if (state.num_nacks > 0) {
        swift_nack_recovery(ALGO_CTX_PASS, &state);
        update_signal(SIG_NACK, -1);
    } else if (state.num_rtos > 0) {
        swift_timeout_recovery(ALGO_CTX_PASS, &state);
        update_signal(SIG_RTO, -1);
    } else if (state.tot_acked > 0) {
        swift_ack_reaction(ALGO_CTX_PASS, &state);
    } else {
        return PCM_ERROR;
    }

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

    update_signal(SIG_ACK, -state.num_acks);
    set_local_state(VAR_ACKED, state.tot_acked);
    set_local_state(VAR_T_LAST_DECREASE, state.t_last_decrease);
    set_local_state(VAR_RTX_CNT, state.retransmit_cnt);
    set_local_state(VAR_RTT_ESTIM, state.rtt_estim);
    set_control(CTRL_CWND, state.cwnd);

    return PCM_SUCCESS;
}