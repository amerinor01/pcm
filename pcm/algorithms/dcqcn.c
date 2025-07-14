#include "dcqcn.h"
#include "algo_utils.h"
#include "assert.h"
#include "pcm.h"

static inline void dcqcn_rate_to_cwnd(struct dcqcn_state_snapshot *state) {
    // Original DCQCN is a rate based, we use RTT to convert rate to cwnd
    // cwnd [bytes] = rate_cur [Gbytes/second] * brtt [picoseconds] * to_bytes
    state->cwnd =
        (pcm_float)state->rate_cur * (pcm_float)state->consts.brtt / 1000.;
}

static inline void dcqcn_rate_decrease(struct dcqcn_state_snapshot *state) {
    /* record old rate as target rate */
    state->rate_target = state->rate_cur;
    /* multiplicative decrease factor */
    state->rate_cur = (pcm_float)state->rate_cur * (1.0 - 0.5 * state->alpha);
    /* update alpha */
    state->alpha =
        (1 - state->consts.gamma) * state->alpha + state->consts.gamma;
    dcqcn_rate_to_cwnd(state);
}

static inline void dcqcn_rate_increase(struct dcqcn_state_snapshot *state) {
    uint32_t min_counter;
    if (MAX(state->rate_increase_timer_evts, state->byte_counter_evts) <
        state->consts.fr_steps) {
        /* Fast Recovery */
        // in fast recovery we are approaching last cached target rate.
    } else if ((min_counter = MIN(state->rate_increase_timer_evts,
                                  state->byte_counter_evts)) >
               state->consts.fr_steps) {
        /* Hyper Increase */
        state->rate_target += min_counter * state->consts.rhai;
    } else {
        /* Additive Increase */
        state->rate_target += state->consts.rai;
    }

    state->rate_cur = (state->rate_target + state->rate_cur) / 2;

    dcqcn_rate_to_cwnd(state);
}

int algorithm_main() {
    struct dcqcn_state_snapshot state = {0};

    state.alpha = get_local_state_float(DCQCN_LOCAL_STATE_IDX_ALPHA);
    state.rate_increase_timer_evts =
        get_local_state_uint(DCQCN_LOCAL_STATE_IDX_RATE_INCREASE_EVTS);
    state.byte_counter_evts =
        get_local_state_uint(DCQCN_LOCAL_STATE_IDX_BYTE_COUNTER_EVTS);
    state.rate_cur = get_local_state_float(DCQCN_LOCAL_STATE_IDX_RATE_CUR);
    state.rate_target =
        get_local_state_float(DCQCN_LOCAL_STATE_IDX_RATE_TARGET);

    state.cwnd = get_control(DCQCN_CTRL_IDX_CWND);

    state.consts.brtt = get_constant_uint(DCQCN_CONST_BRTT);
    state.consts.rai = get_constant_uint(DCQCN_CONST_RAI);
    state.consts.rhai = get_constant_uint(DCQCN_CONST_RHAI);
    state.consts.fr_steps = get_constant_uint(DCQCN_CONST_FR_STEPS);
    state.consts.gamma = get_constant_float(DCQCN_CONST_GAMMA);

    // As a negative signal, ECN's always have higher priority that any other
    // timer
    pcm_uint num_ecns = get_signal(DCQCN_SIG_IDX_ECN);
    if (num_ecns) {
        dcqcn_rate_decrease(&state);
        // we keep track of any excessive ECNs received, so
        // remaining ECN's can be processed immediately by this handler
        // TODO 1: implement reaction to all ECNs here to avoid costly handler
        // re-invocation
        // TODO 2: investigate:
        // update_signal(DCQCN_SIG_IDX_ECN, num_ecns);
        update_signal(DCQCN_SIG_IDX_ECN, -1);
        if (num_ecns - 1 > 0) {
            // disable all timers to avoid rate increase as we haven't consumed
            // all ECNs yet
            set_signal(DCQCN_SIG_IDX_RATE_INCREASE_TIMER, 0);
            set_signal(DCQCN_SIG_IDX_TX_BURST, 0);
            set_signal(DCQCN_SIG_IDX_ALPHA_TIMER, 0);
        } else {
            // this is last (or the only) ECN that was received, arm rate
            // increase timers
            set_local_state_int(DCQCN_LOCAL_STATE_IDX_RATE_INCREASE_EVTS, 0);
            set_local_state_int(DCQCN_LOCAL_STATE_IDX_BYTE_COUNTER_EVTS, 0);
            set_signal(DCQCN_SIG_IDX_RATE_INCREASE_TIMER, 1);
            set_signal(DCQCN_SIG_IDX_TX_BURST, 1);
            set_signal(DCQCN_SIG_IDX_ALPHA_TIMER, 1);
        }
        set_local_state_float(DCQCN_LOCAL_STATE_IDX_RATE_CUR, state.rate_cur);
        set_local_state_float(DCQCN_LOCAL_STATE_IDX_RATE_TARGET,
                              state.rate_target);
        set_local_state_float(DCQCN_LOCAL_STATE_IDX_ALPHA, state.alpha);
        set_control(DCQCN_CTRL_IDX_CWND, state.cwnd);
        goto exit;
    }

    size_t trigger_id = get_signal_invoke_trigger_user_index();
    switch (trigger_id) {
    case DCQCN_SIG_IDX_RATE_INCREASE_TIMER:
        state.rate_increase_timer_evts++;
        dcqcn_rate_increase(&state);
        set_local_state_int(DCQCN_LOCAL_STATE_IDX_RATE_INCREASE_EVTS,
                            state.rate_increase_timer_evts);
        set_local_state_float(DCQCN_LOCAL_STATE_IDX_RATE_CUR, state.rate_cur);
        set_local_state_float(DCQCN_LOCAL_STATE_IDX_RATE_TARGET,
                              state.rate_target);
        set_signal(DCQCN_SIG_IDX_RATE_INCREASE_TIMER, PCM_SIG_REARM);
        set_control(DCQCN_CTRL_IDX_CWND, state.cwnd);
        break;

    case DCQCN_SIG_IDX_TX_BURST:
        state.byte_counter_evts++;
        dcqcn_rate_increase(&state);
        set_local_state_int(DCQCN_LOCAL_STATE_IDX_BYTE_COUNTER_EVTS,
                            state.byte_counter_evts);
        set_local_state_float(DCQCN_LOCAL_STATE_IDX_RATE_CUR, state.rate_cur);
        set_local_state_float(DCQCN_LOCAL_STATE_IDX_RATE_TARGET,
                              state.rate_target);
        set_signal(DCQCN_SIG_IDX_TX_BURST, PCM_SIG_REARM);
        set_control(DCQCN_CTRL_IDX_CWND, state.cwnd);
        break;

    case DCQCN_SIG_IDX_ALPHA_TIMER:
        state.alpha = (1 - state.consts.gamma) * state.alpha;
        set_local_state_float(DCQCN_LOCAL_STATE_IDX_ALPHA, state.alpha);
        set_signal(DCQCN_SIG_IDX_ALPHA_TIMER, PCM_SIG_REARM);
        break;

    default:
        return ERROR;
    }

exit:
    // printf(
    //     "DCQCN post-control: ctx=%p trigger_id=%d num_ecns=%llu rate_tgt=%lf
    //     " "rate_cur=%lf " "alpha=%lf cwnd=%llu\n", ctx, trigger_id, num_ecns,
    //     state.rate_target, state.rate_cur, state.alpha, state.cwnd);

    return SUCCESS;
}