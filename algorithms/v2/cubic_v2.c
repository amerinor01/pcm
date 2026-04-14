#include "cubic_v2.h"
#include "algo_utils.h"
#include "math.h"
#include "pcm.h"
#include "pcmh.h"
#include "tcp_utils.h"

static PCM_FORCE_INLINE void tcp_cubic_perform_decrease(ALGO_CTX_ARGS) {
    pcm_float cwnd_at_loss_mss = get_var_float(CTRL_WINDOW_MSS);
    pcm_float new_sstresh_mss = MAX(BETA * cwnd_at_loss_mss, 2);
    pcm_float new_cwnd_mss = MAX(BETA * cwnd_at_loss_mss, 1);
    pcm_float K = cbrt(((pcm_float)(cwnd_at_loss_mss - new_cwnd_mss)) / C);

    set_var_float(W_MAX_MSS, cwnd_at_loss_mss);
    set_var_float(CTRL_WINDOW_MSS, new_cwnd_mss);
    set_var_float(SSTHRESH, new_sstresh_mss);
    set_var_float(K, K);
    set_var_float(W_EST_MSS, new_cwnd_mss);
    set_var_uint(LAST_ADJ, get_signal(ELAPSED_TIME));
}

static PCM_FORCE_INLINE void tcp_cubic_perform_increase(ALGO_CTX_ARGS, pcm_uint num_acks) {
    pcm_float cwnd_mss = get_var_float(CTRL_WINDOW_MSS);
    pcm_uint RTT_picoseconds = get_signal(RTT_SAMPLE);
    pcm_float RTT_seconds = ((pcm_float)RTT_picoseconds) / 1e12;
    pcm_uint t_picoseconds = get_signal(ELAPSED_TIME) - get_var_uint(LAST_ADJ);
    pcm_float t_seconds = ((pcm_float)t_picoseconds) / 1e12;

    pcm_float w_max = get_var_float(W_MAX_MSS);
    pcm_float K = get_var_float(K);

    pcm_float W_est = get_var_float(W_EST_MSS) + (3 * (1 - BETA) / (1 + BETA)) * (num_acks / cwnd_mss);
    set_var_float(W_EST_MSS, W_est);

    pcm_float factor_t = (t_seconds - K);
    pcm_float w_cubic_t_seconds = C * factor_t * factor_t * factor_t + w_max;

    pcm_float t_and_rtt_seconds = t_seconds + RTT_seconds;
    pcm_float factor_t_and_rtt = (t_and_rtt_seconds - K);
    pcm_float w_cubic_t_and_rtt_seconds = C * factor_t_and_rtt * factor_t_and_rtt * factor_t_and_rtt + w_max;

    if (w_cubic_t_seconds < W_est) {
        set_var_float(CTRL_WINDOW_MSS, W_est);
    } else {
        pcm_float target = w_cubic_t_and_rtt_seconds;
        if (target < cwnd_mss)
            target = cwnd_mss;
        else if (target > (1.5 * cwnd_mss))
            target = (1.5 * cwnd_mss);

        pcm_float new_cwnd = cwnd_mss + ((pcm_float)(target - cwnd_mss)) / cwnd_mss * num_acks;
        set_var_float(CTRL_WINDOW_MSS, new_cwnd);
    }
}

static PCM_FORCE_INLINE void tcp_cubic_slow_start(ALGO_CTX_ARGS, pcm_float *cur_cwnd, pcm_uint *acks_to_consume) {
    pcm_float to_ssthresh = MIN(*acks_to_consume, get_var_float(SSTHRESH) - *cur_cwnd);
    *cur_cwnd += (pcm_uint)to_ssthresh;
    *acks_to_consume -= (pcm_uint)to_ssthresh;
    set_var_float(CTRL_WINDOW_MSS, *cur_cwnd);
    set_var_uint(LAST_ADJ, get_signal(ELAPSED_TIME));
}

int algorithm_main() {
    pcm_uint trigger_mask = get_signal_trigger_mask();
    pcm_float cur_cwnd_mss = get_var_float(CTRL_WINDOW_MSS);

    // Calculate Deltas
    pcm_uint num_ack = get_signal(NUM_ACK) - get_var_uint(PREV_ACK);
    pcm_uint num_ecn = get_signal(NUM_ECN) - get_var_uint(PREV_ECN);

    // Cache current values
    set_var_uint(PREV_ACK, get_signal(NUM_ACK));
    set_var_uint(PREV_ECN, get_signal(NUM_ECN));

    if (trigger_mask & NUM_NACK) {
        tcp_cubic_perform_decrease(ALGO_CTX_PASS);
    } else if (trigger_mask & NUM_ACK) {
        pcm_uint acks_to_consume = num_ack;
        if (num_ecn) {
            tcp_cubic_perform_decrease(ALGO_CTX_PASS);
        } else {
            if (cur_cwnd_mss < get_var_float(SSTHRESH)) {
                tcp_cubic_slow_start(ALGO_CTX_PASS, &cur_cwnd_mss, &acks_to_consume);
            }
            if (acks_to_consume) {
                tcp_cubic_perform_increase(ALGO_CTX_PASS, acks_to_consume);
            }
        }
    }

    set_control(CWND_BYTES, (pcm_uint)(get_var_float(CTRL_WINDOW_MSS) * MSS));

    return PCM_SUCCESS;
}
