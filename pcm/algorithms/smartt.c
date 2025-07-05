#include <stdbool.h>

#include "algo_utils.h"
#include "pcm.h"
#include "smartt.h"

static inline bool smartt_quick_adapt(struct smartt_state_snapshot *state) {
    printf(
        "QA state: now=%d qa_deadline=%d bytes_to_ignore=%d bytes_ignored=%d "
        "acked_bytes=%d trigger_qa=%d\n",
        state->now, state->qa_deadline, state->bytes_to_ignore,
        state->bytes_ignored, state->acked_bytes, state->trigger_qa);
    bool adapted = false;
    if (state->now >= state->qa_deadline) {
        if (state->qa_deadline != 0 && state->trigger_qa) {
            state->trigger_qa = 0;
            adapted = true;
            state->bytes_to_ignore = state->cwnd; // TODO: use inflight bytes
            state->cwnd =
                (pcm_uint)(MAX(state->acked_bytes * state->consts.qa_scaling,
                               state->consts.mss));
            state->bytes_ignored = 0;
        }
        state->acked_bytes = 0;
        state->qa_deadline = state->now + state->consts.trtt;
    }
    return adapted;
}

static inline void
smartt_handle_loss_signal(struct smartt_state_snapshot *state) {
    // state->cwnd -= state->consts.mss; // state->last_pkt_size
    //  SMaRTT paper explicitly mentions that NACKED/TRIMMED/RTO'ed
    //  packet needs to be retransmistted here.
    //  we assume datapath handles this outside
    if (state->bytes_ignored >= state->bytes_to_ignore) {
        state->trigger_qa = 1;
        smartt_quick_adapt(state);
    }
}

static inline bool smartt_fast_increase(struct smartt_state_snapshot *state) {
    printf("FI state last_rtt=%d brtt=%d num_ecns=%d fast_count=%d cwnd=%d "
           "fast_active=%d\n",
           state->last_rtt, state->consts.brtt, state->num_ecns,
           state->fast_count, state->cwnd, state->fast_active);
    if ((ABS((pcm_float)state->last_rtt - state->consts.brtt) <
         (0.75 * (pcm_float)state->consts.brtt)) &&
        !state->num_ecns) {
        state->fast_count += state->consts.mss; // last_pkt_size
        if (state->fast_count > state->cwnd || state->fast_active) {
            // state->cwnd += (pcm_uint)(SMARTT_K_CONST * state->consts.mss);
            state->cwnd += state->consts.mss;
            state->fast_active = 1;
        }
    } else {
        state->fast_count = 0;
        state->fast_active = 0;
    }
    return state->fast_active;
}

static inline void smartt_core_cases(struct smartt_state_snapshot *state) {
    if (!state->num_ecns && state->last_rtt < state->consts.trtt) {
        /* Fair Increase */
        // Case 1 RTT Based Increase
        state->cwnd += (MIN((((state->consts.trtt - state->last_rtt) /
                              (double)state->last_rtt) *
                             state->consts.y_gain * state->consts.mss *
                             (state->consts.mss / (double)state->cwnd)),
                            state->consts.mss)) *
                       state->consts.reaction_delay;
        state->cwnd += ((pcm_float)state->consts.mss / state->cwnd) *
                       state->consts.x_gain * state->consts.mss *
                       state->consts.reaction_delay;
    } else if (state->num_ecns && state->last_rtt > state->consts.trtt) {
        /* Multiplicative Decrease */
        // Case 2 Hybrid Based Decrease || RTT Decrease
        state->cwnd -= state->consts.reaction_delay *
                       MIN(((state->consts.w_gain *
                             (state->last_rtt - (pcm_float)state->consts.trtt) /
                             state->last_rtt * state->consts.mss) +
                            state->cwnd / (double)state->consts.bdp *
                                state->consts.z_gain * state->consts.mss),
                           state->consts.mss);
    } else if (state->num_ecns && state->last_rtt < state->consts.trtt) {
        // Case 3 Gentle Decrease (Window based)
        state->cwnd -= MAX(state->consts.mss,
                           (pcm_float)(state->cwnd) / state->consts.bdp *
                               state->consts.mss * state->consts.z_gain *
                               state->consts.reaction_delay);
        // TBD: request LB path change
    } else if (!state->num_ecns && state->last_rtt > state->consts.trtt) {
        /* Proportional Increase */
        // Case 4 Do nothing but fairness
        state->cwnd += ((pcm_float)state->consts.mss / state->cwnd) *
                       state->consts.x_gain * state->consts.mss *
                       state->consts.reaction_delay;
    }
}

static inline void smartt_handle_ack(struct smartt_state_snapshot *state) {
    state->acked_bytes += state->consts.mss; // state->last_pkt_size

    if (state->bytes_ignored < state->bytes_to_ignore) {
        state->bytes_ignored += state->consts.mss; // state->last_pkt_size
    } else if (!smartt_quick_adapt(state) || !smartt_fast_increase(state)) {
        smartt_core_cases(state);
    }
}

int algorithm_main() {
    struct smartt_state_snapshot state = {0};
    state.num_acks = get_signal(SMARTT_SIG_NUM_ACK);
    state.num_rtos = get_signal(SMARTT_SIG_NUM_RTO);
    state.num_nacks = get_signal(SMARTT_SIG_NUM_NACK);
    state.num_ecns = get_signal(SMARTT_SIG_NUM_ECN);
    state.last_rtt = get_signal(SMARTT_SIG_LAST_RTT);
    state.now = get_signal(SMARTT_SIG_ELAPSED_TIME);

    state.acked_bytes = get_local_state(SMARTT_LOCAL_STATE_ACKED_BYTES);
    state.bytes_ignored = get_local_state(SMARTT_LOCAL_STATE_BYTES_IGNORED);
    state.bytes_to_ignore = get_local_state(SMARTT_LOCAL_STATE_BYTES_TO_IGNORE);
    state.trigger_qa = get_local_state(SMARTT_LOCAL_STATE_TRIGGER_QA);
    state.qa_deadline = get_local_state(SMARTT_LOCAL_STATE_QA_DEADLINE);
    state.fast_count = get_local_state(SMARTT_LOCAL_STATE_FAST_COUNT);
    state.fast_active = get_local_state(SMARTT_LOCAL_STATE_FAST_ACTIVE);

    state.cwnd = get_control(SMARTT_CTRL_CWND_BYTES);

    state.consts.bdp = 252300;
    state.consts.brtt = 5058000;
    state.consts.trtt = 7637580;
    state.consts.mss = 2048;
    state.consts.x_gain = 2.0;
    state.consts.y_gain = 2.5;
    state.consts.z_gain = 2;
    state.consts.w_gain = 0.8;
    state.consts.reaction_delay = 1.0;
    state.consts.qa_scaling = 1;

    // state.last_rtt = state.last_rtt; // we don't support avg RTT (yet)
    printf("num_nacks=%d num_rtos=%d num_acks=%d, cwnd=%d\n", state.num_nacks,
           state.num_rtos, state.num_acks, state.cwnd);

    if (state.num_nacks > 0) {
        smartt_handle_loss_signal(&state);
        update_signal(SMARTT_SIG_NUM_NACK, -1);
        goto save_state;
    }

    if (state.num_rtos > 0) {
        smartt_handle_loss_signal(&state);
        update_signal(SMARTT_SIG_NUM_RTO, -1);
        goto save_state;
    }

    if (state.num_acks > 0) {
        smartt_handle_ack(&state);
        update_signal(SMARTT_SIG_NUM_ACK, -1);
        if (state.num_ecns) {
            update_signal(SMARTT_SIG_NUM_ECN, -1);
        }
    }

save_state:

    set_control(SMARTT_CTRL_CWND_BYTES, state.cwnd);

    set_local_state(SMARTT_LOCAL_STATE_ACKED_BYTES, state.acked_bytes);
    set_local_state(SMARTT_LOCAL_STATE_BYTES_IGNORED, state.bytes_ignored);
    set_local_state(SMARTT_LOCAL_STATE_BYTES_TO_IGNORE, state.bytes_to_ignore);
    set_local_state(SMARTT_LOCAL_STATE_TRIGGER_QA, state.trigger_qa);
    set_local_state(SMARTT_LOCAL_STATE_QA_DEADLINE, state.qa_deadline);
    set_local_state(SMARTT_LOCAL_STATE_FAST_COUNT, state.fast_count);
    set_local_state(SMARTT_LOCAL_STATE_FAST_ACTIVE, state.fast_active);

    return SUCCESS;
}