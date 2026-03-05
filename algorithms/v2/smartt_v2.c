#include "smartt_v2.h"
#include "algo_utils.h"
#include "pcm.h"
#include "pcmh.h"
#include <stdbool.h>

static PCM_FORCE_INLINE bool smartt_quick_adapt(ALGO_CTX_ARGS, pcm_uint t_now, pcm_uint *cur_cwnd) {
    bool adapted = false;
    if (t_now >= get_var_uint(VAR_QA_DEADLINE)) {
        if (get_var_uint(VAR_QA_DEADLINE) != 0 && get_var_uint(VAR_TRIGGER_QA)) {
            set_var_uint(VAR_TRIGGER_QA, 0);
            adapted = true;
            set_var_uint(VAR_BYTES_TO_IGNORE, *cur_cwnd);
            *cur_cwnd = (pcm_uint)(MAX(get_var_uint(VAR_ACKED_BYTES) * QA_SCALING, (pcm_uint)MSS));
            set_var_uint(VAR_BYTES_IGNORED, 0);
        }
        set_var_uint(VAR_ACKED_BYTES, 0);
        set_var_uint(VAR_QA_DEADLINE, t_now + TRTT);
    }
    return adapted;
}

static PCM_FORCE_INLINE void smartt_handle_loss_signal(ALGO_CTX_ARGS, pcm_uint t_now, pcm_uint *cur_cwnd) {
    if (get_var_uint(VAR_BYTES_IGNORED) >= get_var_uint(VAR_BYTES_TO_IGNORE)) {
        set_var_uint(VAR_TRIGGER_QA, 1);
        smartt_quick_adapt(ALGO_CTX_PASS, t_now, cur_cwnd);
    }
}

static PCM_FORCE_INLINE bool smartt_fast_increase(ALGO_CTX_ARGS, pcm_uint num_ecns, pcm_uint rtt_sample, pcm_uint *cur_cwnd) {
    if ((ABS((pcm_float)rtt_sample - BRTT) < (FI_BRTT_TOL * (pcm_float)BRTT)) && !num_ecns) {
        set_var_uint(VAR_FAST_COUNT, get_var_uint(VAR_FAST_COUNT) + MSS);
        if (get_var_uint(VAR_FAST_COUNT) > *cur_cwnd || get_var_uint(VAR_FAST_ACTIVE)) {
            *cur_cwnd += MSS;
            set_var_uint(VAR_FAST_ACTIVE, 1);
        }
    } else {
        set_var_uint(VAR_FAST_COUNT, 0);
        set_var_uint(VAR_FAST_ACTIVE, 0);
    }
    return get_var_uint(VAR_FAST_ACTIVE);
}

static PCM_FORCE_INLINE void smartt_core_cases(pcm_uint num_ecns, pcm_uint rtt_sample, pcm_uint *cur_cwnd) {
    if (!num_ecns && rtt_sample < TRTT) {
        *cur_cwnd += (MIN((((TRTT - rtt_sample) / (double)rtt_sample) * Y_GAIN * MSS * (MSS / (double)*cur_cwnd)), MSS)) * REACTION_DELAY;
        *cur_cwnd += ((pcm_float)MSS / *cur_cwnd) * X_GAIN * MSS * REACTION_DELAY;
    } else if (num_ecns && rtt_sample > TRTT) {
        *cur_cwnd -= REACTION_DELAY *
                     MIN(((W_GAIN * (rtt_sample - (pcm_float)TRTT) / rtt_sample * MSS) + *cur_cwnd / (double)BDP * Z_GAIN * MSS), MSS);
    } else if (num_ecns && rtt_sample < TRTT) {
        *cur_cwnd -= MAX(MSS, (pcm_float)(*cur_cwnd) / BDP * MSS * Z_GAIN * REACTION_DELAY);
    } else if (!num_ecns && rtt_sample > TRTT) {
        *cur_cwnd += ((pcm_float)MSS / *cur_cwnd) * X_GAIN * MSS * REACTION_DELAY;
    }
}

static PCM_FORCE_INLINE void smartt_handle_ack(ALGO_CTX_ARGS, pcm_uint num_ecns, pcm_uint rtt_sample, pcm_uint t_now, pcm_uint *cur_cwnd) {
    set_var_uint(VAR_ACKED_BYTES, get_var_uint(VAR_ACKED_BYTES) + MSS);

    if (get_var_uint(VAR_BYTES_IGNORED) < get_var_uint(VAR_BYTES_TO_IGNORE)) {
        set_var_uint(VAR_BYTES_IGNORED, get_var_uint(VAR_BYTES_IGNORED) + MSS);
    } else if (!smartt_quick_adapt(ALGO_CTX_PASS, t_now, cur_cwnd) ||
               !smartt_fast_increase(ALGO_CTX_PASS, num_ecns, rtt_sample, cur_cwnd)) {
        smartt_core_cases(num_ecns, rtt_sample, cur_cwnd);
    }
}

int algorithm_main() {
    pcm_uint rtt_sample = get_signal(SIG_RTT_SAMPLE);
    pcm_uint t_now = get_signal(SIG_ELAPSED_TIME);
    pcm_uint cur_cwnd = get_control(CTRL_CWND_BYTES);

    // Calculate Deltas
    pcm_uint num_nack = get_signal(SIG_NUM_NACK) - get_var_uint(VAR_PREV_NACK);
    pcm_uint num_rto = get_signal(SIG_NUM_RTO) - get_var_uint(VAR_PREV_RTO);
    pcm_uint num_ack = get_signal(SIG_NUM_ACK) - get_var_uint(VAR_PREV_ACK);
    pcm_uint num_ecns = get_signal(SIG_NUM_ECN) - get_var_uint(VAR_PREV_ECN);

    // Update Cache
    set_var_uint(VAR_PREV_NACK, get_signal(SIG_NUM_NACK));
    set_var_uint(VAR_PREV_RTO, get_signal(SIG_NUM_RTO));
    set_var_uint(VAR_PREV_ACK, get_signal(SIG_NUM_ACK));
    set_var_uint(VAR_PREV_ECN, get_signal(SIG_NUM_ECN));

    if (num_nack > 0) {
        smartt_handle_loss_signal(ALGO_CTX_PASS, t_now, &cur_cwnd);
        goto save_state;
    }

    if (num_rto > 0) {
        smartt_handle_loss_signal(ALGO_CTX_PASS, t_now, &cur_cwnd);
        goto save_state;
    }

    if (num_ack > 0) {
        smartt_handle_ack(ALGO_CTX_PASS, num_ecns, rtt_sample, t_now, &cur_cwnd);
    }

save_state:
    set_control(CTRL_CWND_BYTES, cur_cwnd);

    return PCM_SUCCESS;
}
