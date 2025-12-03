#include "cubic.h"
#include "algo_utils.h"
#include "math.h"
#include "pcm.h"
#include "pcmh.h"
#include "tcp_utils.h"

static PCM_FORCE_INLINE void tcp_cubic_perform_decrease(ALGO_CTX_ARGS) {
    pcm_float cwnd_at_loss_mss = get_var_float(VAR_CTRL_WINDOW_MSS);
    // replace to be constant
    // get another beta to represent ECN/NACK diff

    pcm_float new_sstresh_mss = MAX(BETA * cwnd_at_loss_mss, 2);
    pcm_float new_cwnd_mss = MAX(BETA * cwnd_at_loss_mss, 1);

    // fast implementation for cubic root required
    // - implementation that always terminates
    // - doesnt rely on cbrt
    pcm_float K = cbrt(((pcm_float)(cwnd_at_loss_mss - new_cwnd_mss)) / C);
    set_var_float(VAR_W_MAX_MSS, cwnd_at_loss_mss);
    set_var_float(VAR_CTRL_WINDOW_MSS, new_cwnd_mss);
    set_var_float(VAR_SSTHRESH, new_sstresh_mss);
    set_var_float(VAR_K, K);
    set_var_float(VAR_W_EST_MSS, new_cwnd_mss);
    set_var_uint(VAR_LAST_ADJ, get_signal(SIG_ELAPSED_TIME)); // update last time a congestion event occured
}

static PCM_FORCE_INLINE void tcp_cubic_perform_increase(ALGO_CTX_ARGS, pcm_uint num_acks) {
    pcm_float cwnd_mss = get_var_float(VAR_CTRL_WINDOW_MSS);
    // W_cubic(t+RTT)

    pcm_uint RTT_picoseconds = get_signal(SIG_RTT_SAMPLE);
    pcm_float RTT_seconds = ((pcm_float)RTT_picoseconds) / 1e12;
    pcm_uint t_picoseconds = get_signal(SIG_ELAPSED_TIME) - get_var_uint(VAR_LAST_ADJ);
    pcm_float t_seconds = ((pcm_float)t_picoseconds) / 1e12;

    pcm_float w_max = get_var_float(VAR_W_MAX_MSS);
    pcm_float K = get_var_float(VAR_K);
    // check if it is a TCP friendly region
    pcm_float W_est = get_var_float(VAR_W_EST_MSS) + (3 * (1 - BETA) / (1 + BETA)) * (num_acks / cwnd_mss);
    set_var_float(VAR_W_EST_MSS, W_est);

    pcm_float factor_t = (t_seconds - K);
    pcm_float w_cubic_t_seconds = C * factor_t * factor_t * factor_t + w_max;

    pcm_float t_and_rtt_seconds = t_seconds + RTT_seconds;
    pcm_float factor_t_and_rtt = (t_and_rtt_seconds - K);
    pcm_float w_cubic_t_and_rtt_seconds = C * factor_t_and_rtt * factor_t_and_rtt * factor_t_and_rtt + w_max;
    // add ifdef/for tcp friendliness
    // TCP friendly condition
    if (w_cubic_t_seconds < W_est) {
        set_var_float(VAR_CTRL_WINDOW_MSS, W_est);
    } else {
        // non TCP friendly region
        pcm_float target = w_cubic_t_and_rtt_seconds;
        if (target < cwnd_mss) {
            target = cwnd_mss;
            // printf("upped but leads to 0\n value factor %f\n", factor);
        } else if (target > (1.5 * cwnd_mss)) {
            target = (1.5 * cwnd_mss);
            // printf("capped\n value factor %f\n", factor);
        } else {
            // printf("doing well\n value factor %f\n", factor);
        }

        pcm_float new_cwnd = cwnd_mss + ((pcm_float)(target - cwnd_mss)) / cwnd_mss * num_acks;
        set_var_float(VAR_CTRL_WINDOW_MSS, new_cwnd);
    }
}

PCM_FORCE_INLINE void tcp_cubic_slow_start(ALGO_CTX_ARGS, pcm_float *cur_cwnd, pcm_uint *acks_to_consume) {
    pcm_float to_ssthresh = MIN(*acks_to_consume, get_var_float(VAR_SSTHRESH) - *cur_cwnd);
    // printf("to_ssthresh %f\n", to_ssthresh);
    // printf("VAR_SSTHRESH %f\n", get_var_float(VAR_SSTHRESH));
    // printf("*cur_cwnd %f\n", *cur_cwnd);
    // printf("*acks_to_consume %d\n", *acks_to_consume);
    // printf("\n\n\n");
    *cur_cwnd += (pcm_uint)to_ssthresh;
    *acks_to_consume -= (pcm_uint)to_ssthresh;
    set_var_float(VAR_CTRL_WINDOW_MSS, *cur_cwnd);
    set_var_uint(VAR_LAST_ADJ, get_signal(SIG_ELAPSED_TIME)); // update last time a congestion event occured
}

int algorithm_main() {
    pcm_uint trigger_mask = get_signal_trigger_mask();
    pcm_float cur_cwnd_mss = get_var_float(VAR_CTRL_WINDOW_MSS);
    // printf("VAR_CTRL_WINDOW_MSS %f\n", cur_cwnd_mss);

    if (trigger_mask & SIG_NUM_NACK) {
        tcp_cubic_perform_decrease(ALGO_CTX_PASS);

        update_signal(SIG_NUM_NACK, -get_signal(SIG_NUM_NACK));

    } else if (trigger_mask & SIG_NUM_ACK) {
        pcm_uint num_acks = get_signal(SIG_NUM_ACK);
        pcm_uint acks_to_consume = num_acks;
        if (get_signal(SIG_NUM_ECN)) {
            tcp_cubic_perform_decrease(ALGO_CTX_PASS);
            update_signal(SIG_NUM_ECN, -get_signal(SIG_NUM_ECN));
        } else {
            if (cur_cwnd_mss < get_var_float(VAR_SSTHRESH)) {
                tcp_cubic_slow_start(ALGO_CTX_PASS, &cur_cwnd_mss, &acks_to_consume);
                update_signal(SIG_NUM_ACK, -(num_acks - acks_to_consume));
            }
            if (acks_to_consume) {
                tcp_cubic_perform_increase(ALGO_CTX_PASS, acks_to_consume);
            }
        }
        update_signal(SIG_NUM_ACK, -acks_to_consume);
    }

    set_control(CTRL_CWND_BYTES, (pcm_uint)(get_var_float(VAR_CTRL_WINDOW_MSS) * MSS));

    return PCM_SUCCESS;
}