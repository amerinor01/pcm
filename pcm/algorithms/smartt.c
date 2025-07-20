#include <stdbool.h>

#include "algo_utils.h"
#include "pcm.h"
#include "smartt.h"

static PCM_FORCE_INLINE bool smartt_quick_adapt(ALGO_CTX_ARGS, pcm_uint t_now, pcm_uint *cur_cwnd) {
    // printf("SMaRTT QA state: now=%llu qa_deadline=%llu bytes_to_ignore=%llu "
    //        "bytes_ignored=%llu "
    //        "acked_bytes=%llu trigger_qa=%llu\n",
    //        state->now, state->qa_deadline, state->bytes_to_ignore,
    //        state->bytes_ignored, state->acked_bytes, state->trigger_qa);
    bool adapted = false;
    if (t_now >= get_var_uint(VAR_QA_DEADLINE)) {
        if (get_var_uint(VAR_QA_DEADLINE) != 0 && get_var_uint(VAR_TRIGGER_QA)) {
            set_var_uint(VAR_TRIGGER_QA, 0);
            adapted = true;
            set_var_uint(VAR_BYTES_TO_IGNORE,
                         *cur_cwnd); // TODO: use inflight bytes
            *cur_cwnd = (pcm_uint)(MAX(get_var_uint(VAR_ACKED_BYTES) * QA_SCALING, MSS));
            set_var_uint(VAR_BYTES_IGNORED, 0);
        }
        set_var_uint(VAR_ACKED_BYTES, 0);
        set_var_uint(VAR_QA_DEADLINE, t_now + TRTT);
    }
    return adapted;
}

static PCM_FORCE_INLINE void smartt_handle_loss_signal(ALGO_CTX_ARGS, pcm_uint t_now, pcm_uint *cur_cwnd) {
    // *cur_cwnd -= TRTT; // state->last_pkt_size
    //  SMaRTT paper explicitly mentions that NACKED/TRIMMED/RTO'ed
    //  packet needs to be retransmistted here.
    //  we assume datapath handles this outside
    if (get_var_uint(VAR_BYTES_IGNORED) >= get_var_uint(VAR_BYTES_TO_IGNORE)) {
        set_var_uint(VAR_TRIGGER_QA, 1);
        smartt_quick_adapt(ALGO_CTX_PASS, t_now, cur_cwnd);
    }
}

static PCM_FORCE_INLINE bool smartt_fast_increase(ALGO_CTX_ARGS, pcm_uint num_ecns, pcm_uint rtt_sample, pcm_uint *cur_cwnd) {
    // printf(
    //     "SMaRTT FI state rtt_sample=%llu brtt=%llu num_ecns=%llu
    //     fast_count=%llu cwnd=%llu " "fast_active=%llu\n", rtt_sample,
    //     BRTT, num_ecns, state->fast_count, *cur_cwnd,
    //     state->fast_active);
    if ((ABS((pcm_float)rtt_sample - BRTT) < (FI_BRTT_TOL * (pcm_float)BRTT)) && !num_ecns) {
        set_var_uint(VAR_FAST_COUNT,
                     get_var_uint(VAR_FAST_COUNT) + MSS); // last_pkt_size
        if (get_var_uint(VAR_FAST_COUNT) > *cur_cwnd || get_var_uint(VAR_FAST_ACTIVE)) {
            // *cur_cwnd += (pcm_uint)(SMARTT_K_CONST * MSS);
            *cur_cwnd += MSS;
            set_var_uint(VAR_FAST_ACTIVE, 1);
        }
    } else {
        set_var_uint(VAR_FAST_COUNT, 0);
        set_var_uint(VAR_FAST_ACTIVE, 0);
    }
    return get_var_uint(VAR_FAST_ACTIVE);
}

static PCM_FORCE_INLINE void smartt_core_cases(ALGO_CTX_ARGS, pcm_uint num_ecns, pcm_uint rtt_sample, pcm_uint *cur_cwnd) {
    if (!num_ecns && rtt_sample < TRTT) {
        /* Fair Increase */
        // Case 1 RTT Based Increase
        *cur_cwnd += (MIN((((TRTT - rtt_sample) / (double)rtt_sample) * Y_GAIN * MSS * (MSS / (double)*cur_cwnd)), MSS)) * REACTION_DELAY;
        *cur_cwnd += ((pcm_float)MSS / *cur_cwnd) * X_GAIN * MSS * REACTION_DELAY;
    } else if (num_ecns && rtt_sample > TRTT) {
        /* Multiplicative Decrease */
        // Case 2 Hybrid Based Decrease || RTT Decrease
        *cur_cwnd -= REACTION_DELAY *
                     MIN(((W_GAIN * (rtt_sample - (pcm_float)TRTT) / rtt_sample * MSS) + *cur_cwnd / (double)BDP * Z_GAIN * MSS), MSS);
    } else if (num_ecns && rtt_sample < TRTT) {
        // Case 3 Gentle Decrease (Window based)
        *cur_cwnd -= MAX(MSS, (pcm_float)(*cur_cwnd) / BDP * MSS * Z_GAIN * REACTION_DELAY);
        // TBD: request LB path change
    } else if (!num_ecns && rtt_sample > TRTT) {
        /* Proportional Increase */
        // Case 4 Do nothing but fairness
        *cur_cwnd += ((pcm_float)MSS / *cur_cwnd) * X_GAIN * MSS * REACTION_DELAY;
    }
}

static PCM_FORCE_INLINE void smartt_handle_ack(ALGO_CTX_ARGS, pcm_uint num_ecns, pcm_uint rtt_sample, pcm_uint t_now, pcm_uint *cur_cwnd) {
    // TODO: last_pkt_size, not MSS?
    set_var_uint(VAR_ACKED_BYTES, get_var_uint(VAR_ACKED_BYTES) + MSS);

    if (get_var_uint(VAR_BYTES_IGNORED) < get_var_uint(VAR_BYTES_TO_IGNORE)) {
        set_var_uint(VAR_BYTES_IGNORED,
                     get_var_uint(VAR_BYTES_IGNORED) + MSS); // TODO: += last_pkt_size
    } else if (!smartt_quick_adapt(ALGO_CTX_PASS, t_now, cur_cwnd) ||
               !smartt_fast_increase(ALGO_CTX_PASS, num_ecns, rtt_sample, cur_cwnd)) {
        smartt_core_cases(ALGO_CTX_PASS, num_ecns, rtt_sample, cur_cwnd);
    }
}

int algorithm_main() {
    pcm_uint rtt_sample = get_signal(SIG_RTT_SAMPLE); // we don't use/support avg RTT (yet)
    pcm_uint t_now = get_signal(SIG_ELAPSED_TIME);
    pcm_uint cur_cwnd = get_control(CTRL_CWND_BYTES);

    if (get_signal(SIG_NUM_NACK) > 0) {
        smartt_handle_loss_signal(ALGO_CTX_PASS, t_now, &cur_cwnd);
        update_signal(SIG_NUM_NACK, -1);
        goto save_state;
    }

    if (get_signal(SIG_NUM_RTO) > 0) {
        smartt_handle_loss_signal(ALGO_CTX_PASS, t_now, &cur_cwnd);
        update_signal(SIG_NUM_RTO, -1);
        goto save_state;
    }

    if (get_signal(SIG_NUM_ACK) > 0) {
        pcm_uint num_ecns = get_signal(SIG_NUM_ECN);
        smartt_handle_ack(ALGO_CTX_PASS, num_ecns, rtt_sample, t_now, &cur_cwnd);
        update_signal(SIG_NUM_ACK, -1);
        if (num_ecns) {
            update_signal(SIG_NUM_ECN, -1);
        }
    }

save_state:

    // printf("SMaRTT: num_nacks=%llu num_rtos=%llu num_acks=%llu, cwnd=%llu\n",
    //        state.num_nacks, state.num_rtos, state.num_acks, state.cwnd);

    set_control(CTRL_CWND_BYTES, cur_cwnd);

    return PCM_SUCCESS;
}