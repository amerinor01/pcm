#include <math.h>
#include <stdbool.h>

#include "pcmh.h"
#include "algo_utils.h"
#include "swift.h"

static PCM_FORCE_INLINE pcm_float swift_target_delay(pcm_uint cur_cwnd) {
    pcm_uint fs_delay = FS_ALPHA / sqrtf((pcm_float)cur_cwnd / MSS) + FS_BETA;

    if (fs_delay > FS_RANGE)
        fs_delay = FS_RANGE;

    if (fs_delay < 0.0)
        fs_delay = 0.0;

    // if (!add_fs_delay) // SwiftSrc::targetDelay in htsim sim goes into this branch for PLB
    //     fs_delay = 0.0; //

    pcm_uint hop_delay = HOP_COUNT * H;
    return BRTT + fs_delay + hop_delay;
}

static PCM_FORCE_INLINE void swift_ack_reaction(ALGO_CTX_ARGS, pcm_uint num_acks, pcm_uint rtt_sample, bool can_decrease,
                                                pcm_uint *cur_cwnd) {
    set_var_uint(VAR_RETX_CNT, 0);
    //set_var_uint(VAR_ACKED, get_var(VAR_ACKED) + num_acks * MSS);
    pcm_float target_delay = swift_target_delay(*cur_cwnd);

    if (rtt_sample < target_delay) {
        /* Additive Increase */
        //*cur_cwnd += MSS * AI * (get_var_uint(VAR_ACKED) / *cur_cwnd);
        *cur_cwnd += (MSS * AI * num_acks * MSS) / *cur_cwnd;
        // Note: we DO NOT support FP fractional cwnd (yet)
        // if (state->cwnd >= 1) {
        //    state->cwnd += (MSS * AI * num_acks * MSS) / *cur_cwnd;
        //} else {
        //    state->cwnd += AI * num_acks * MSS;
        //}
    } else {
        /* Multiplicative Decrease */
        if (can_decrease) {
            pcm_float mdf = BETA * ((pcm_float)(rtt_sample - target_delay) / (pcm_float)rtt_sample);
            *cur_cwnd = (pcm_float)(*cur_cwnd) * MAX(1.0 - mdf, 1.0 - MAX_MDF);
        }
    }
}

static PCM_FORCE_INLINE void swift_rtt_estimate(ALGO_CTX_ARGS, pcm_uint rtt_sample) {
    if (get_var_uint(VAR_RTT_ESTIM) > 0) {
        set_var_uint(VAR_RTT_ESTIM, 7 * get_var_uint(VAR_RTT_ESTIM) / 8 + rtt_sample / 8);
    } else {
        set_var_uint(VAR_RTT_ESTIM, rtt_sample);
    }
}

static PCM_FORCE_INLINE void swift_nack_recovery(ALGO_CTX_ARGS, bool can_decrease, pcm_uint *cur_cwnd) {
    set_var_uint(VAR_RETX_CNT, 0);
    if (can_decrease) {
        *cur_cwnd = (pcm_float)(*cur_cwnd) * (1.0 - MAX_MDF);
    }
}

// TODO: test me in RTO-based simulation
static PCM_FORCE_INLINE void swift_timeout_recovery(ALGO_CTX_ARGS, bool can_decrease, pcm_uint *cur_cwnd) {
    set_var_uint(VAR_RETX_CNT, get_var_uint(VAR_RETX_CNT) + 1);
    if (get_var_uint(VAR_RETX_CNT) >= RETX_RESET_THRESH) {
        *cur_cwnd = MSS; // minimum possible cwnd
    } else if (can_decrease) {
        *cur_cwnd = (pcm_float)(*cur_cwnd) * (1.0 - MAX_MDF);
    }
}

/*
 * Swift CC loop without support for fast recovery detection and
 * pacer delay and FP cwnd
 * Based on the htsim implementation of Swift:
 *
 * https://github.com/Broadcom/csg-htsim/blob/67cbbbb1cccdbddc400803414127d044ecd55290/sim/swift_new.cpp#L71
 *
 *  TODO: support remote processing delay part of Swift
 *  TODO: support pacer delay
 */
int algorithm_main() {
    pcm_uint t_now = get_signal(SIG_ELAPSED_TIME);
    pcm_uint rtt_sample = get_signal(SIG_RTT);
    pcm_uint num_acks = get_signal(SIG_ACK);
    pcm_uint cur_cwnd = get_control(CTRL_CWND);
    pcm_uint prev_cwnd = cur_cwnd;

    bool can_decrease = (t_now - get_var_uint(VAR_T_LAST_DECREASE)) >= get_var(VAR_RTT_ESTIM);

    /*
     * Note: for some reason htsim computes RTT estimation after computing
     * can_decrease predicate.
     */
    swift_rtt_estimate(ALGO_CTX_PASS, rtt_sample);

    if (get_signal(SIG_NACK) > 0) {
        swift_nack_recovery(ALGO_CTX_PASS, can_decrease, &cur_cwnd);
        update_signal(SIG_NACK, -1);
    } else if (get_signal(SIG_RTO) > 0) {
        swift_timeout_recovery(ALGO_CTX_PASS, can_decrease, &cur_cwnd);
        update_signal(SIG_RTO, -1);
    } else if (get_signal(SIG_ACK) > 0) {
        swift_ack_reaction(ALGO_CTX_PASS, num_acks, rtt_sample, can_decrease, &cur_cwnd);
    } else {
        return PCM_ERROR;
    }

    // All acks are always consumed ACKs
    update_signal(SIG_ACK, -num_acks);

    /*
     * Update last‐decrease timestamp if we actually shrank
     * and reset RTT measurement
     */
    if (cur_cwnd < prev_cwnd) {
        set_var_uint(VAR_T_LAST_DECREASE, t_now);
        //set_var_uint(VAR_ACKED, 0);
    }

    // if (cur_cwnd >= 1.0) {
    //    set_control(CTRL_PACER_DELAY, rtt_sample / cur_cwnd);
    //} else {
    //    set_control(CTRL_PACER_DELAY, 0);
    //}

    set_control(CTRL_CWND, cur_cwnd);

    return PCM_SUCCESS;
}