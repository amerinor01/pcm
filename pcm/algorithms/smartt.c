#include "smartt.h"
#include "algo_utils.h"
#include "pcm.h"

static inline int smartt_quick_adapt(struct smartt_state_snapshot *state) {
    int adapted = 0;
    if (state->now >= state->qa_deadline) {
        if (state->trigger_qa) {
            state->trigger_qa = 0;
            adapted = 1;
            state->cwnd = (int)(MAX(state->acked_bytes, FABRIC_LINK_MTU) *
                                SMARTT_QA_SCALING);
            state->bytes_to_ignore = state->cwnd;
            state->bytes_ignored = 0;
        }
        state->qa_deadline = state->now + (int)SMARTT_QA_DEADLINE;
        state->acked_bytes = 0;
    }
    return adapted;
}

static inline int smartt_fast_increase(struct smartt_state_snapshot *state) {
    if (ABS((float)state->last_rtt - FABRIC_BASE_RTT) < SMARTT_FI_TOL &&
        !state->num_ecns) {
        state->fast_count += FABRIC_LINK_MTU; // last_pkt_size
        if (state->fast_count > state->cwnd || state->fast_active) {
            state->cwnd += (int)(SMARTT_K_CONST * FABRIC_LINK_MTU);
            state->fast_active = 1;
        }
    } else {
        state->fast_count = 0;
        state->fast_active = 0;
    }
    return state->fast_active;
}

static inline void smartt_core_cases(struct smartt_state_snapshot *state) {
    if (state->num_ecns && state->last_rtt <= SMARTT_TARGET_RTT) {
        // TBD: request LB path change
    } else if (state->num_ecns && state->last_rtt > SMARTT_TARGET_RTT) {
        /* Multiplicative Decrease */
        float mdf = 1.0 - ((float)state->avg_rtt - SMARTT_TARGET_RTT) /
                              (float)state->avg_rtt * SMARTT_MD_CONST;
        state->cwnd = (int)((float)state->cwnd * MAX(0.5, mdf));
    } else if (!state->num_ecns && state->last_rtt > SMARTT_TARGET_RTT) {
        /* Fair Increase */
        state->cwnd += (FABRIC_LINK_MTU / (float)state->cwnd) *
                       (float)(FABRIC_LINK_MTU * SMARTT_FI_CONST);
    } else if (!state->num_ecns && state->last_rtt <= SMARTT_TARGET_RTT) {
        /* Proportional Increase */
        int increase =
            ((SMARTT_TARGET_RTT - state->last_rtt) / state->last_rtt) *
            FABRIC_LINK_MTU / state->cwnd *
            (int)(FABRIC_LINK_MTU * (float)SMARTT_PI_CONST);
        state->cwnd += MIN(FABRIC_LINK_MTU, increase);
    }
}

static inline void
smartt_handle_loss_signal(struct smartt_state_snapshot *state) {
    state->cwnd -= FABRIC_LINK_MTU; // state->last_pkt_size
    state->trigger_qa = 1;
    // SMaRTT paper explicitly mentions that NACKED/TRIMMED/RTO'ed
    // packet needs to be retransmistted here.
    // we assume datapath handles this outside
    if (state->bytes_ignored >= state->bytes_to_ignore) {
        smartt_quick_adapt(state);
    }
}

static inline void smartt_handle_ack(struct smartt_state_snapshot *state) {
    state->acked_bytes += FABRIC_LINK_MTU; // state->last_pkt_size

    if (state->bytes_ignored < state->bytes_to_ignore) {
        state->bytes_ignored += FABRIC_LINK_MTU; // state->last_pkt_size
    } else if (!(smartt_quick_adapt(state) || smartt_fast_increase(state))) {
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

    state.avg_rtt = state.last_rtt; // we don't support avg RTT (yet)

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
    // clamp cwnd
    state.cwnd = MAX(MIN(state.cwnd, FABRIC_MAX_CWND), FABRIC_MIN_CWND);
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