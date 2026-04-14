#include "swift_v2.h"
#include "algo_utils.h"
#include "pcmh.h"
#include <math.h>
#include <stdbool.h>

static PCM_FORCE_INLINE pcm_float swift_target_delay(pcm_uint cur_cwnd) {
    pcm_uint fs_delay = FS_ALPHA / sqrtf((pcm_float)cur_cwnd / MSS) + FS_BETA;
    if (fs_delay > FS_RANGE)
        fs_delay = FS_RANGE;
    if (fs_delay < 0.0)
        fs_delay = 0.0;
    pcm_uint hop_delay = HOP_COUNT * H;
    return BRTT + fs_delay + hop_delay;
}

static PCM_FORCE_INLINE void swift_ack_reaction(ALGO_CTX_ARGS, pcm_uint num_acks, pcm_uint rtt_sample, bool can_decrease,
                                                pcm_uint *cur_cwnd) {
    set_var_uint(RETX_CNT, 0);
    pcm_float target_delay = swift_target_delay(*cur_cwnd);

    if (rtt_sample < target_delay) {
        *cur_cwnd += (MSS * AI * num_acks * MSS) / *cur_cwnd;
    } else {
        if (can_decrease) {
            pcm_float mdf = BETA * ((pcm_float)(rtt_sample - target_delay) / (pcm_float)rtt_sample);
            *cur_cwnd = (pcm_float)(*cur_cwnd) * MAX(1.0 - mdf, 1.0 - MAX_MDF);
        }
    }
}

static PCM_FORCE_INLINE void swift_rtt_estimate(ALGO_CTX_ARGS, pcm_uint rtt_sample) {
    if (get_var_uint(RTT_ESTIM) > 0) {
        set_var_uint(RTT_ESTIM, 7 * get_var_uint(RTT_ESTIM) / 8 + rtt_sample / 8);
    } else {
        set_var_uint(RTT_ESTIM, rtt_sample);
    }
}

static PCM_FORCE_INLINE void swift_nack_recovery(ALGO_CTX_ARGS, bool can_decrease, pcm_uint *cur_cwnd) {
    set_var_uint(RETX_CNT, 0);
    if (can_decrease) {
        *cur_cwnd = (pcm_float)(*cur_cwnd) * (1.0 - MAX_MDF);
    }
}

static PCM_FORCE_INLINE void swift_timeout_recovery(ALGO_CTX_ARGS, bool can_decrease, pcm_uint *cur_cwnd) {
    set_var_uint(RETX_CNT, get_var_uint(RETX_CNT) + 1);
    if (get_var_uint(RETX_CNT) >= RETX_RESET_THRESH) {
        *cur_cwnd = MSS;
    } else if (can_decrease) {
        *cur_cwnd = (pcm_float)(*cur_cwnd) * (1.0 - MAX_MDF);
    }
}

int algorithm_main() {
    pcm_uint t_now = get_signal(ELAPSED_TIME);
    pcm_uint rtt_sample = get_signal(RTT);
    pcm_uint cur_cwnd = get_control(CWND);
    pcm_uint prev_cwnd = cur_cwnd;

    // Calculate Deltas
    pcm_uint num_nack = get_signal(NACK) - get_var_uint(PREV_NACK);
    pcm_uint num_rto = get_signal(RTO) - get_var_uint(PREV_RTO);
    pcm_uint num_ack = get_signal(ACK) - get_var_uint(PREV_ACK);

    // Update Cache
    set_var_uint(PREV_NACK, get_signal(NACK));
    set_var_uint(PREV_RTO, get_signal(RTO));
    set_var_uint(PREV_ACK, get_signal(ACK));

    bool can_decrease = (t_now - get_var_uint(T_LAST_DECREASE)) >= get_var(RTT_ESTIM);

    swift_rtt_estimate(ALGO_CTX_PASS, rtt_sample);

    if (num_nack > 0) {
        swift_nack_recovery(ALGO_CTX_PASS, can_decrease, &cur_cwnd);
    } else if (num_rto > 0) {
        swift_timeout_recovery(ALGO_CTX_PASS, can_decrease, &cur_cwnd);
    } else if (num_ack > 0) {
        swift_ack_reaction(ALGO_CTX_PASS, num_ack, rtt_sample, can_decrease, &cur_cwnd);
    } else {
        return PCM_ERROR;
    }

    if (cur_cwnd < prev_cwnd) {
        set_var_uint(T_LAST_DECREASE, t_now);
    }

    set_control(CWND, cur_cwnd);

    return PCM_SUCCESS;
}
