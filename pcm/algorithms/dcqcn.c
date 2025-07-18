#include "dcqcn.h"
#include "algo_utils.h"
#include "assert.h"
#include "pcm.h"

static PCM_FORCE_INLINE void dcqcn_rate_to_cwnd(ALGO_CTX_ARGS,
                                                pcm_uint *cwnd_cur) {
    // Original DCQCN is a rate based, we use RTT to convert rate to cwnd
    // cwnd [bytes] = rate_cur [Gbytes/second] * brtt [picoseconds] * to_bytes
    *cwnd_cur = get_var_float(VAR_CUR_RATE) * CONST_BRTT / 1000.;
}

static PCM_FORCE_INLINE void dcqcn_rate_decrease(ALGO_CTX_ARGS,
                                                 pcm_uint *cwnd_cur) {
    /* record old rate as target rate */
    set_var_float(VAR_TGT_RATE, get_var_float(VAR_CUR_RATE));
    /* multiplicative decrease factor */
    set_var_float(VAR_CUR_RATE, get_var_float(VAR_CUR_RATE) *
                                    (1.0 - 0.5 * get_var_float(VAR_ALPHA)));
    /* update alpha */
    set_var_float(VAR_ALPHA,
                  (1 - CONST_GAMMA) * get_var_float(VAR_ALPHA) + CONST_GAMMA);
    dcqcn_rate_to_cwnd(ALGO_CTX_PASS, cwnd_cur);
}

static PCM_FORCE_INLINE void dcqcn_rate_increase(ALGO_CTX_ARGS,
                                                 pcm_uint *cwnd_cur) {
    uint32_t min_counter;
    if (MAX(get_var_uint(VAR_RATE_INCREASE_EVTS),
            get_var_uint(VAR_BYTE_COUNTER_EVTS)) < CONST_FR_STEPS) {
        /* Fast Recovery */
        // in fast recovery we are approaching last cached target rate.
    } else if ((min_counter = MIN(get_var_uint(VAR_RATE_INCREASE_EVTS),
                                  get_var_uint(VAR_BYTE_COUNTER_EVTS))) >
               CONST_FR_STEPS) {
        /* Hyper Increase */
        set_var_float(VAR_TGT_RATE,
                      get_var_float(VAR_TGT_RATE) + min_counter * CONST_RHAI);
    } else {
        /* Additive Increase */
        set_var_float(VAR_TGT_RATE, get_var_float(VAR_TGT_RATE) + CONST_RAI);
    }

    set_var_float(VAR_CUR_RATE,
                  (get_var_float(VAR_TGT_RATE) + get_var_float(VAR_CUR_RATE)) /
                      2);

    dcqcn_rate_to_cwnd(ALGO_CTX_PASS, cwnd_cur);
}

int algorithm_main() {
    pcm_uint cur_cwnd = get_control(CTRL_CWND);

    // As a negative signal, ECN's always have higher priority that any other
    // timer
    pcm_uint num_ecns = get_signal(SIG_ECN);
    if (num_ecns) {
        dcqcn_rate_decrease(ALGO_CTX_PASS, &cur_cwnd);
        // we keep track of any excessive ECNs received, so
        // remaining ECN's can be processed immediately by this handler
        // TODO 1: implement reaction to all ECNs here to avoid costly handler
        // re-invocation
        // TODO 2: investigate:
        // update_signal(SIG_ECN, num_ecns);
        update_signal(SIG_ECN, -1);
        if (num_ecns - 1 > 0) {
            // disable all timers to avoid rate increase as we haven't consumed
            // all ECNs yet
            set_signal(SIG_RATE_INCREASE_TIMER, 0);
            set_signal(SIG_TX_BURST, 0);
            set_signal(SIG_ALPHA_TIMER, 0);
        } else {
            // this is last (or the only) ECN that was received, arm rate
            // increase timers
            set_var_int(VAR_RATE_INCREASE_EVTS, 0);
            set_var_int(VAR_BYTE_COUNTER_EVTS, 0);
            set_signal(SIG_RATE_INCREASE_TIMER, 1);
            set_signal(SIG_TX_BURST, 1);
            set_signal(SIG_ALPHA_TIMER, 1);
        }

        goto save_cwnd_and_exit;
    }

    size_t trigger_id = get_signal_invoke_trigger_user_index();
    switch (trigger_id) {
    case SIG_RATE_INCREASE_TIMER:
        set_var_uint(VAR_RATE_INCREASE_EVTS,
                     get_var_uint(VAR_RATE_INCREASE_EVTS) + 1);
        dcqcn_rate_increase(ALGO_CTX_PASS, &cur_cwnd);
        set_signal(SIG_RATE_INCREASE_TIMER, PCM_SIG_REARM);
        break;

    case SIG_TX_BURST:
        set_var_uint(VAR_BYTE_COUNTER_EVTS,
                     get_var_uint(VAR_BYTE_COUNTER_EVTS) + 1);
        dcqcn_rate_increase(ALGO_CTX_PASS, &cur_cwnd);
        set_signal(SIG_TX_BURST, PCM_SIG_REARM);
        break;

    case SIG_ALPHA_TIMER:
        set_var_float(VAR_ALPHA, (1 - CONST_GAMMA) * get_var_float(VAR_ALPHA));
        set_signal(SIG_ALPHA_TIMER, PCM_SIG_REARM);
        goto exit;

    default:
        return PCM_ERROR;
    }

save_cwnd_and_exit:
    set_control(CTRL_CWND, cur_cwnd);
exit:
    // fprintf(stderr,
    //     "DCQCN post-control: ctx=%p trigger_id=%d num_ecns=%llu rate_tgt=%lf
    //     " "rate_cur=%lf " "alpha=%lf cwnd=%llu\n", ctx, trigger_id, num_ecns,
    //     state.rate_target, state.rate_cur, state.alpha, state.cwnd);

    return PCM_SUCCESS;
}