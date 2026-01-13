#include <stdbool.h>

#include "pcmh.h"
#include "algo_utils.h"
#include "nscc.h"
#include "pcm.h"

/*
 * nscc_update_q_delay(...) is inspired by the update_delay(..) in htsim.
 *
 * We do it in handler now for two reasons:
 * 1) HTSIM logic in update_delay(..) is based on unclear assuptions: RTT sample for NACKs is
 * network_rtt+base_rtt sum while for ACKs it's sample in packet. 2) We want to match htsim's NSCC
 * implementation as close as possible for debugging purposes.
 *
 * Ideally, PCM-enabled datapath/NIC should be able to do the EWMA in hardware and make handler
 * lighter.
 */
static PCM_FORCE_INLINE pcm_uint nscc_get_avg_q_delay(ALGO_CTX_ARGS) {
    return get_var_uint(VAR_AVG_Q_DELAY);
}
static PCM_FORCE_INLINE void nscc_update_q_delay(ALGO_CTX_ARGS, bool ecn) {
    pcm_uint new_avg_q_delay;
    pcm_uint q_delay =
        (get_signal(SIG_RTT_SAMPLE) > BRTT) ? (get_signal(SIG_RTT_SAMPLE) - BRTT) : 0;
    if (!ecn && q_delay > TARGET_Q_DELAY) {
        new_avg_q_delay =
            EWMA_DELAY_ALPHA * BRTT * 0.25 + (1 - EWMA_DELAY_ALPHA) * get_var_uint(VAR_AVG_Q_DELAY);
    } else {
        if (q_delay > 5 * BRTT) {
            new_avg_q_delay = 0.0125 * q_delay + (1 - 0.0125) * get_var_uint(VAR_AVG_Q_DELAY);
        } else {
            new_avg_q_delay =
                EWMA_DELAY_ALPHA * q_delay + (1 - EWMA_DELAY_ALPHA) * get_var_uint(VAR_AVG_Q_DELAY);
        }
    }
    set_var_uint(VAR_AVG_Q_DELAY, new_avg_q_delay);
}

static PCM_FORCE_INLINE void nscc_fair_increase(ALGO_CTX_ARGS) {
    pcm_float increment = FI * get_signal(SIG_NUM_ACKED_BYTES);
    set_var_uint(VAR_FULFILL_ADJ_INCREMENT, get_var_uint(VAR_FULFILL_ADJ_INCREMENT) + increment);
}

static PCM_FORCE_INLINE void nscc_fast_increase(ALGO_CTX_ARGS, pcm_uint q_delay,
                                                pcm_uint *cur_cwnd) {
    if (q_delay < FI_QDELAY_TOL) {
        set_var_uint(VAR_FAST_COUNT,
                     get_var_uint(VAR_FAST_COUNT) + get_signal(SIG_NUM_ACKED_BYTES));
        if (get_var_uint(VAR_FAST_COUNT) > *cur_cwnd || get_var_uint(VAR_FAST_ACTIVE)) {
            *cur_cwnd += get_signal(SIG_NUM_ACKED_BYTES) * FI_SCALE;
            set_var_uint(VAR_FAST_ACTIVE, 1);
            return;
        }
    } else {
        set_var_uint(VAR_FAST_COUNT, 0);
    }
    set_var_uint(VAR_FAST_ACTIVE, 0);
}

static PCM_FORCE_INLINE void nscc_proportional_increase(ALGO_CTX_ARGS, pcm_uint q_delay,
                                                        pcm_uint *cur_cwnd) {
    nscc_fast_increase(ALGO_CTX_PASS, q_delay, cur_cwnd);
    if (get_var_uint(VAR_FAST_ACTIVE))
        return;

    pcm_float cwnd_increment =
        PI_ALPHA * (pcm_float)(get_signal(SIG_NUM_ACKED_BYTES) * (TARGET_Q_DELAY - q_delay));
    set_var_uint(VAR_FULFILL_ADJ_INCREMENT,
                 get_var_uint(VAR_FULFILL_ADJ_INCREMENT) + cwnd_increment);
}

static PCM_FORCE_INLINE void nscc_multiplicative_decrease(ALGO_CTX_ARGS, pcm_uint *cur_cwnd) {
    set_var_uint(VAR_FAST_ACTIVE, 0);
    set_var_uint(VAR_FAST_COUNT, 0);
    if (nscc_get_avg_q_delay(ALGO_CTX_PASS) > TARGET_Q_DELAY) {
        if (get_signal(SIG_ELAPSED_TIME) - get_var_uint(VAR_T_MD_LAST) > BRTT) {
            pcm_float decrease_factor = MAX(
                1.0 - MD_GAMMA * (pcm_float)(nscc_get_avg_q_delay(ALGO_CTX_PASS) - TARGET_Q_DELAY) /
                          (pcm_float)nscc_get_avg_q_delay(ALGO_CTX_PASS),
                0.5);
            *cur_cwnd = MAX((pcm_uint)(*cur_cwnd * decrease_factor), (pcm_uint)MSS);
            set_var_uint(VAR_T_MD_LAST, get_signal(SIG_ELAPSED_TIME));
        }
    }
}

static PCM_FORCE_INLINE void nscc_core_cases(ALGO_CTX_ARGS, pcm_uint q_delay, pcm_uint *cur_cwnd) {
    if (!get_signal(SIG_NUM_ECN) && q_delay >= TARGET_Q_DELAY) {
        // printf("Core case: branch=preFI _cwnd=%llu, num_acked_bytes=%llu\n", *cur_cwnd,
        //        get_signal(SIG_NUM_ACKED_BYTES));
        nscc_fair_increase(ALGO_CTX_PASS);
        // printf("Core case: branch=postFI _cwnd=%llu\n", *cur_cwnd);
        // printf("DEBUG LOGGING: Core case: flow=%p t_now=%llu branch=FI _cwnd=%llu\n", ctx,
        //        get_signal(SIG_ELAPSED_TIME), *cur_cwnd);
    } else if (!get_signal(SIG_NUM_ECN) && q_delay < TARGET_Q_DELAY) {
        // printf("Core case: branch=prePI _cwnd=%llu, num_acked_bytes=%llu\n", *cur_cwnd,
        //        get_signal(SIG_NUM_ACKED_BYTES));
        nscc_proportional_increase(ALGO_CTX_PASS, q_delay, cur_cwnd);
        // printf("Core case: branch=postPI _cwnd=%llu\n", *cur_cwnd);
        // printf("DEBUG LOGGING: Core case: flow=%p t_now=%llu branch=PI _cwnd=%llu\n", ctx,
        //        get_signal(SIG_ELAPSED_TIME), *cur_cwnd);
    } else if (get_signal(SIG_NUM_ECN) && q_delay >= TARGET_Q_DELAY) {
        // printf("Core case: branch=postMD _cwnd=%llu, num_acked_bytes=%llu\n", *cur_cwnd,
        //        get_signal(SIG_NUM_ACKED_BYTES));
        nscc_multiplicative_decrease(ALGO_CTX_PASS, cur_cwnd);
        // printf("Core case: branch=preMD _cwnd=%llu\n", *cur_cwnd);
        // printf("DEBUG LOGGING: Core case: flow=%p t_now=%llu branch=MD _cwnd=%llu\n", ctx,
        //        get_signal(SIG_ELAPSED_TIME), *cur_cwnd);
    } else if (get_signal(SIG_NUM_ECN) && q_delay < TARGET_Q_DELAY) {
        // NOOP, just switch path
        // printf("DEBUG LOGGING: Core case: flow=%p t_now=%llu branch=NOOP _cwnd=%llu\n", ctx,
        //        get_signal(SIG_ELAPSED_TIME), *cur_cwnd);
        // printf("Core case: branch=NOOP _cwnd=%llu, num_acked_bytes=%llu q_delay=%llu "
        //        "tgt_q_delay=%llu rtt_sample=%llu\n",
        //        *cur_cwnd, get_signal(SIG_NUM_ACKED_BYTES), q_delay, TARGET_Q_DELAY,
        //        get_signal(SIG_RTT_SAMPLE));
    }
}

static PCM_FORCE_INLINE bool nscc_quick_adapt(ALGO_CTX_ARGS, pcm_uint q_delay, bool got_ecn,
                                              bool is_loss, pcm_uint ignore_bytes,
                                              pcm_uint *cur_cwnd) {
    set_var_uint(VAR_QA_BYTES_IGNORED, get_var_uint(VAR_QA_BYTES_IGNORED) + ignore_bytes);

    bool qa_done_or_ignore = false;
    if ((get_var_uint(VAR_QA_BYTES_IGNORED) < get_var_uint(VAR_QA_BYTES_TO_IGNORE)) && got_ecn) {
        qa_done_or_ignore = true;
    } else if (get_signal(SIG_ELAPSED_TIME) >= get_var_uint(VAR_QA_DEADLINE)) {
        if (get_var_uint(VAR_QA_DEADLINE) != 0 &&
            (get_var_uint(VAR_QA_TRIGGER) || is_loss || (q_delay > QA_THRESHOLD)) &&
            get_var_uint(VAR_QA_ACKED_BYTES) < (BDP >> QA_GATE)) {
            set_var_uint(VAR_QA_TRIGGER, 0);
            set_var_uint(VAR_QA_BYTES_TO_IGNORE, get_signal(SIG_IN_FLIGHT_BYTES));
            set_var_uint(VAR_QA_BYTES_IGNORED, 0);
            *cur_cwnd = (pcm_uint)(MAX(get_var_uint(VAR_QA_ACKED_BYTES), (pcm_uint)MSS));
            qa_done_or_ignore = true;
        }
        set_var_uint(VAR_QA_ACKED_BYTES, 0);
        set_var_uint(VAR_QA_DEADLINE, get_signal(SIG_ELAPSED_TIME) + TRTT);
    }

    if (qa_done_or_ignore) {
        set_var_uint(VAR_FULFILL_ADJ_INCREMENT, 0);
        set_var_uint(VAR_FULFILL_ADJ_BYTES, 0);
    }

    return qa_done_or_ignore;
}

static PCM_FORCE_INLINE void nscc_handle_ack(ALGO_CTX_ARGS, pcm_uint q_delay, pcm_uint *cur_cwnd) {
    set_var_uint(VAR_QA_ACKED_BYTES,
                 get_var_uint(VAR_QA_ACKED_BYTES) + get_signal(SIG_NUM_ACKED_BYTES));
    set_var_uint(VAR_FULFILL_ADJ_BYTES,
                 get_var_uint(VAR_FULFILL_ADJ_BYTES) + get_signal(SIG_NUM_ACKED_BYTES));

    if (nscc_quick_adapt(ALGO_CTX_PASS, q_delay, get_signal(SIG_NUM_ECN), false,
                         get_signal(SIG_NUM_ACKED_BYTES), cur_cwnd)) {
        //printf("DEBUG LOGGING: Core case: flow=%p t_now=%llu branch=QA-ACK _cwnd=%llu\n", ctx,
        //       get_signal(SIG_ELAPSED_TIME), *cur_cwnd);
        return;
    }

    nscc_core_cases(ALGO_CTX_PASS, q_delay, cur_cwnd);
}

static PCM_FORCE_INLINE void nscc_handle_loss_signal(ALGO_CTX_ARGS, pcm_uint *cur_cwnd) {
    set_var_uint(VAR_QA_TRIGGER, 1);

    if (!nscc_quick_adapt(ALGO_CTX_PASS, 0, true, true, get_signal(SIG_NUM_NACKED_BYTES),
                          cur_cwnd)) { // && (!_receiver_based_cc || !last_hop)) {
        *cur_cwnd -= get_signal(SIG_NUM_NACKED_BYTES);
        // printf("DEBUG LOGGING: Core case: flow=%p t_now=%llu branch=LOSS _cwnd=%llu\n", ctx,
        //        get_signal(SIG_ELAPSED_TIME), *cur_cwnd);
    } else {
        // printf("DEBUG LOGGING: Core case: flow=%p t_now=%llu branch=QA-LOSS _cwnd=%llu\n", ctx,
        //        get_signal(SIG_ELAPSED_TIME), *cur_cwnd);
    }
}

static PCM_FORCE_INLINE void nscc_fulfill_adjustment(ALGO_CTX_ARGS, pcm_uint *cur_cwnd) {
    if ((get_var_uint(VAR_FULFILL_ADJ_BYTES) > FULFILL_ADJ_BYTES_THRESH) ||
        (get_signal(SIG_ELAPSED_TIME) - get_var_uint(VAR_LAST_FULFILL_ADJ_TIME)) >
            FULFILL_ADJ_TIME_THRESH) {
        *cur_cwnd += get_var_uint(VAR_FULFILL_ADJ_INCREMENT) / *cur_cwnd;
        if ((get_signal(SIG_ELAPSED_TIME) - get_var_uint(VAR_LAST_FULFILL_ADJ_TIME)) >
            FULFILL_ADJ_TIME_THRESH) {
            cur_cwnd += (pcm_uint)ETA;
            set_var_uint(VAR_LAST_FULFILL_ADJ_TIME, get_signal(SIG_ELAPSED_TIME));
        }
        set_var_uint(VAR_FULFILL_ADJ_BYTES, 0);
        set_var_uint(VAR_FULFILL_ADJ_INCREMENT, 0);
        // TO CLARIFY: spec doesn't do the reset below, but htsim does. Who's right?
        set_var_uint(VAR_LAST_FULFILL_ADJ_TIME, get_signal(SIG_ELAPSED_TIME));
    }
}

int algorithm_main() {
    pcm_uint q_delay;
    if (get_signal(SIG_RTT_SAMPLE) >= BRTT) {
        nscc_update_q_delay(ALGO_CTX_PASS, get_signal(SIG_NUM_ECN));
        q_delay = get_signal(SIG_RTT_SAMPLE) - BRTT;
    } else {
        q_delay = nscc_get_avg_q_delay(ALGO_CTX_PASS);
    }

    pcm_uint cur_cwnd = get_control(CTRL_CWND_BYTES);

    pcm_uint trigger_mask = get_signal_trigger_mask();
    if (trigger_mask & SIG_NUM_NACKED_BYTES) {
        nscc_handle_loss_signal(ALGO_CTX_PASS, &cur_cwnd);
        update_signal(SIG_NUM_NACKED_BYTES, -get_signal(SIG_NUM_NACKED_BYTES));
        goto save_state;
    } else if (trigger_mask & SIG_NUM_ACKED_BYTES) {
        nscc_handle_ack(ALGO_CTX_PASS, q_delay, &cur_cwnd);
        nscc_fulfill_adjustment(ALGO_CTX_PASS, &cur_cwnd);
        update_signal(SIG_NUM_ACKED_BYTES, -get_signal(SIG_NUM_ACKED_BYTES));
        if (get_signal(SIG_NUM_ECN)) {
            update_signal(SIG_NUM_ECN, -get_signal(SIG_NUM_ECN));
        }
    } else {
        return PCM_ERROR;
    }

save_state:
    set_control(CTRL_CWND_BYTES, cur_cwnd);
    return PCM_SUCCESS;
}