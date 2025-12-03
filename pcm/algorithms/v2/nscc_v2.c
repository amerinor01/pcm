#include "nscc_v2.h"
#include "algo_utils.h"
#include "pcm.h"
#include "pcmh.h"
#include <stdbool.h>

static PCM_FORCE_INLINE pcm_uint nscc_get_avg_q_delay(ALGO_CTX_ARGS) { return get_var_uint(VAR_AVG_Q_DELAY); }
static PCM_FORCE_INLINE void nscc_update_q_delay(ALGO_CTX_ARGS, bool ecn) {
    pcm_uint new_avg_q_delay;
    pcm_uint q_delay = (get_signal(SIG_RTT_SAMPLE) > BRTT) ? (get_signal(SIG_RTT_SAMPLE) - BRTT) : 0;
    if (!ecn && q_delay > TARGET_Q_DELAY) {
        new_avg_q_delay = EWMA_DELAY_ALPHA * BRTT * 0.25 + (1 - EWMA_DELAY_ALPHA) * get_var_uint(VAR_AVG_Q_DELAY);
    } else {
        if (q_delay > 5 * BRTT) {
            new_avg_q_delay = 0.0125 * q_delay + (1 - 0.0125) * get_var_uint(VAR_AVG_Q_DELAY);
        } else {
            new_avg_q_delay = EWMA_DELAY_ALPHA * q_delay + (1 - EWMA_DELAY_ALPHA) * get_var_uint(VAR_AVG_Q_DELAY);
        }
    }
    set_var_uint(VAR_AVG_Q_DELAY, new_avg_q_delay);
}

static PCM_FORCE_INLINE void nscc_fair_increase(ALGO_CTX_ARGS, pcm_uint newly_acked_bytes) {
    pcm_float increment = FI * newly_acked_bytes;
    set_var_uint(VAR_FULFILL_ADJ_INCREMENT, get_var_uint(VAR_FULFILL_ADJ_INCREMENT) + increment);
}

static PCM_FORCE_INLINE void nscc_fast_increase(ALGO_CTX_ARGS, pcm_uint q_delay, pcm_uint *cur_cwnd, pcm_uint newly_acked_bytes) {
    if (q_delay < FI_QDELAY_TOL) {
        set_var_uint(VAR_FAST_COUNT, get_var_uint(VAR_FAST_COUNT) + newly_acked_bytes);
        if (get_var_uint(VAR_FAST_COUNT) > *cur_cwnd || get_var_uint(VAR_FAST_ACTIVE)) {
            *cur_cwnd += newly_acked_bytes * FI_SCALE;
            set_var_uint(VAR_FAST_ACTIVE, 1);
            return;
        }
    } else {
        set_var_uint(VAR_FAST_COUNT, 0);
    }
    set_var_uint(VAR_FAST_ACTIVE, 0);
}

static PCM_FORCE_INLINE void nscc_proportional_increase(ALGO_CTX_ARGS, pcm_uint q_delay, pcm_uint *cur_cwnd, pcm_uint newly_acked_bytes) {
    nscc_fast_increase(ALGO_CTX_PASS, q_delay, cur_cwnd, newly_acked_bytes);
    if (get_var_uint(VAR_FAST_ACTIVE))
        return;

    pcm_float cwnd_increment = PI_ALPHA * (pcm_float)(newly_acked_bytes * (TARGET_Q_DELAY - q_delay));
    set_var_uint(VAR_FULFILL_ADJ_INCREMENT, get_var_uint(VAR_FULFILL_ADJ_INCREMENT) + cwnd_increment);
}

static PCM_FORCE_INLINE void nscc_multiplicative_decrease(ALGO_CTX_ARGS, pcm_uint *cur_cwnd) {
    set_var_uint(VAR_FAST_ACTIVE, 0);
    set_var_uint(VAR_FAST_COUNT, 0);
    if (nscc_get_avg_q_delay(ALGO_CTX_PASS) > TARGET_Q_DELAY) {
        if (get_signal(SIG_ELAPSED_TIME) - get_var_uint(VAR_T_MD_LAST) > BRTT) {
            pcm_float decrease_factor = MAX(1.0 - MD_GAMMA * (pcm_float)(nscc_get_avg_q_delay(ALGO_CTX_PASS) - TARGET_Q_DELAY) /
                                                      (pcm_float)nscc_get_avg_q_delay(ALGO_CTX_PASS),
                                            0.5);
            *cur_cwnd = MAX((pcm_uint)(*cur_cwnd * decrease_factor), (pcm_uint)MSS);
            set_var_uint(VAR_T_MD_LAST, get_signal(SIG_ELAPSED_TIME));
        }
    }
}

static PCM_FORCE_INLINE void nscc_core_cases(ALGO_CTX_ARGS, pcm_uint q_delay, pcm_uint *cur_cwnd, pcm_uint num_ecn,
                                             pcm_uint newly_acked_bytes) {
    if (!num_ecn && q_delay >= TARGET_Q_DELAY) {
        nscc_fair_increase(ALGO_CTX_PASS, newly_acked_bytes);
    } else if (!num_ecn && q_delay < TARGET_Q_DELAY) {
        nscc_proportional_increase(ALGO_CTX_PASS, q_delay, cur_cwnd, newly_acked_bytes);
    } else if (num_ecn && q_delay >= TARGET_Q_DELAY) {
        nscc_multiplicative_decrease(ALGO_CTX_PASS, cur_cwnd);
    }
}

static PCM_FORCE_INLINE bool nscc_quick_adapt(ALGO_CTX_ARGS, pcm_uint q_delay, bool got_ecn, bool is_loss, pcm_uint ignore_bytes,
                                              pcm_uint *cur_cwnd) {
    set_var_uint(VAR_QA_BYTES_IGNORED, get_var_uint(VAR_QA_BYTES_IGNORED) + ignore_bytes);

    bool qa_done_or_ignore = false;
    if ((get_var_uint(VAR_QA_BYTES_IGNORED) < get_var_uint(VAR_QA_BYTES_TO_IGNORE)) && got_ecn) {
        qa_done_or_ignore = true;
    } else if (get_signal(SIG_ELAPSED_TIME) >= get_var_uint(VAR_QA_DEADLINE)) {
        if (get_var_uint(VAR_QA_DEADLINE) != 0 && (get_var_uint(VAR_QA_TRIGGER) || is_loss || (q_delay > QA_THRESHOLD)) &&
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

static PCM_FORCE_INLINE void nscc_handle_ack(ALGO_CTX_ARGS, pcm_uint q_delay, pcm_uint *cur_cwnd, pcm_uint delta_acked_bytes,
                                             pcm_uint delta_ecn) {
    set_var_uint(VAR_QA_ACKED_BYTES, get_var_uint(VAR_QA_ACKED_BYTES) + delta_acked_bytes);
    set_var_uint(VAR_FULFILL_ADJ_BYTES, get_var_uint(VAR_FULFILL_ADJ_BYTES) + delta_acked_bytes);

    if (nscc_quick_adapt(ALGO_CTX_PASS, q_delay, delta_ecn > 0, false, delta_acked_bytes, cur_cwnd)) {
        return;
    }

    nscc_core_cases(ALGO_CTX_PASS, q_delay, cur_cwnd, delta_ecn, delta_acked_bytes);
}

static PCM_FORCE_INLINE void nscc_handle_loss_signal(ALGO_CTX_ARGS, pcm_uint *cur_cwnd, pcm_uint delta_nacked_bytes) {
    set_var_uint(VAR_QA_TRIGGER, 1);

    if (!nscc_quick_adapt(ALGO_CTX_PASS, 0, true, true, delta_nacked_bytes, cur_cwnd)) {
        *cur_cwnd -= delta_nacked_bytes;
    }
}

static PCM_FORCE_INLINE void nscc_fulfill_adjustment(ALGO_CTX_ARGS, pcm_uint *cur_cwnd) {
    if ((get_var_uint(VAR_FULFILL_ADJ_BYTES) > FULFILL_ADJ_BYTES_THRESH) ||
        (get_signal(SIG_ELAPSED_TIME) - get_var_uint(VAR_LAST_FULFILL_ADJ_TIME)) > FULFILL_ADJ_TIME_THRESH) {
        *cur_cwnd += get_var_uint(VAR_FULFILL_ADJ_INCREMENT) / *cur_cwnd;
        if ((get_signal(SIG_ELAPSED_TIME) - get_var_uint(VAR_LAST_FULFILL_ADJ_TIME)) > FULFILL_ADJ_TIME_THRESH) {
            *cur_cwnd += (pcm_uint)ETA;
            set_var_uint(VAR_LAST_FULFILL_ADJ_TIME, get_signal(SIG_ELAPSED_TIME));
        }
        set_var_uint(VAR_FULFILL_ADJ_BYTES, 0);
        set_var_uint(VAR_FULFILL_ADJ_INCREMENT, 0);
        set_var_uint(VAR_LAST_FULFILL_ADJ_TIME, get_signal(SIG_ELAPSED_TIME));
    }
}

int algorithm_main() {
    pcm_uint q_delay;
    // Calculate Deltas
    pcm_uint delta_nack = get_signal(SIG_NUM_NACK) - get_var_uint(VAR_PREV_NACK);
    pcm_uint delta_ack = get_signal(SIG_NUM_ACK) - get_var_uint(VAR_PREV_ACK);
    pcm_uint delta_ecn = get_signal(SIG_NUM_ECN) - get_var_uint(VAR_PREV_ECN);
    pcm_uint delta_nacked_bytes = get_signal(SIG_NUM_NACKED_BYTES) - get_var_uint(VAR_PREV_NACKED_BYTES);
    pcm_uint delta_acked_bytes = get_signal(SIG_NUM_ACKED_BYTES) - get_var_uint(VAR_PREV_ACKED_BYTES);

    // Update Cache
    set_var_uint(VAR_PREV_NACK, get_signal(SIG_NUM_NACK));
    set_var_uint(VAR_PREV_ACK, get_signal(SIG_NUM_ACK));
    set_var_uint(VAR_PREV_ECN, get_signal(SIG_NUM_ECN));
    set_var_uint(VAR_PREV_NACKED_BYTES, get_signal(SIG_NUM_NACKED_BYTES));
    set_var_uint(VAR_PREV_ACKED_BYTES, get_signal(SIG_NUM_ACKED_BYTES));

    if (get_signal(SIG_RTT_SAMPLE) >= BRTT) {
        nscc_update_q_delay(ALGO_CTX_PASS, delta_ecn > 0);
        q_delay = get_signal(SIG_RTT_SAMPLE) - BRTT;
    } else {
        q_delay = nscc_get_avg_q_delay(ALGO_CTX_PASS);
    }

    pcm_uint cur_cwnd = get_control(CTRL_CWND_BYTES);

    if (delta_nack > 0) {
        nscc_handle_loss_signal(ALGO_CTX_PASS, &cur_cwnd, delta_nacked_bytes);
        goto save_state;
    } else if (delta_ack > 0) {
        nscc_handle_ack(ALGO_CTX_PASS, q_delay, &cur_cwnd, delta_acked_bytes, delta_ecn);
        nscc_fulfill_adjustment(ALGO_CTX_PASS, &cur_cwnd);
    } else {
        return PCM_ERROR;
    }

save_state:
    set_control(CTRL_CWND_BYTES, cur_cwnd);
    return PCM_SUCCESS;
}
