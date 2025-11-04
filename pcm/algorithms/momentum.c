#include "pcmh.h"
#include "algo_utils.h"
#include "pcm.h"
#include "momentum.h"

int algorithm_main() {
    pcm_uint trigger_mask = get_signal_trigger_mask();
    if (trigger_mask & SIG_NUM_NACK) {
        pcm_uint cur_cwnd =
            MAX((pcm_uint)MINIMUM_CWND_BYTES, get_control(CTRL_CWND_BYTES) - get_signal(SIG_NUM_NACKED_BYTES));
        set_control(CTRL_CWND_BYTES, cur_cwnd);
        // printf("EAILD: newly_nacked_bytes=%llu\n", get_signal(SIG_NUM_NACKED_BYTES), get_signal(SIG_NUM_NACK) * MSS);
        update_signal(SIG_NUM_NACK, -get_signal(SIG_NUM_NACK));
        update_signal(SIG_NUM_NACKED_BYTES, -get_signal(SIG_NUM_NACKED_BYTES));
        return PCM_SUCCESS;
    } else {
        pcm_uint cur_cwnd = get_control(CTRL_CWND_BYTES) - MINIMUM_CWND_BYTES; // offset by minimum cwnd
        // printf("EAILD: curr_cwnd=%llu num_acked_bytes=%llu num_ecn=%llu num_ack=%llu ",
        //        cur_cwnd, get_signal(SIG_NUM_ACKED_BYTES), get_signal(SIG_NUM_ECN), get_signal(SIG_NUM_ACK));
        pcm_uint time_stamp = get_signal(SIG_TIME_STAMP);
        pcm_uint dt_picosec  = time_stamp - get_var_uint(VAR_LAST_TRIGGERED_TIME);
        if (dt_picosec  == time_stamp) {
            // the flow has just started, force starting from 0
            cur_cwnd = 0;
        } else {
            pcm_float dt = (pcm_float)(dt_picosec) / 1e9; // in miliseconds
            pcm_float dw_dt = LOG_ALPHA * cur_cwnd;
            // printf("dw_dt(exp)=%f ", dw_dt);

            pcm_uint num_ack = get_signal(SIG_NUM_ACK);
            pcm_uint num_ecn = get_signal(SIG_NUM_ECN);
            pcm_float ecn_portion = (pcm_float)num_ecn / num_ack;
            dw_dt -= ecn_portion * BETA;
            // printf("dw_dt(lin decay)=%f", - ecn_portion * BETA);

            pcm_uint bytes_sent = get_signal(SIG_NUM_ACKED_BYTES);
            // pcm_uint bytes_sent = get_var_uint(VAR_ACKED_BYTES);
            pcm_uint bytes_to_send = get_signal(SIG_BYTES_TO_SEND);
            pcm_float to_send_portion = (pcm_float)bytes_to_send / (bytes_sent + bytes_to_send);
            // printf(" sent_portion=%f", sent_portion);
            pcm_float effective_gamma = GAMMA_MIN + (1 - POW(to_send_portion, GAMMA_POWER)) * (GAMMA_MAX - GAMMA_MIN);
            dw_dt += (1 - ecn_portion) * effective_gamma;
            // printf(" dw_dt(lin recov)=%f", (1 - ecn_portion) * effective_gamma);

            // dw_dt += fma(- effective_gamma - BETA, ecn_portion, effective_gamma);

            pcm_int dw = (pcm_int)(dw_dt * dt);
            if (dw < 0) {
                dw = -dw;
                if (dw > cur_cwnd) {
                    cur_cwnd = 0;
                } else {
                    cur_cwnd -= dw;
                }
            } else {
                cur_cwnd += dw;
            }
            // printf(" dw=%llu\n", dw);
        }
        set_control(CTRL_CWND_BYTES, MIN(cur_cwnd + MINIMUM_CWND_BYTES, MAXIMUM_CWND_BYTES));
        set_var_uint(VAR_LAST_TRIGGERED_TIME, time_stamp);
        set_signal(SIG_NUM_ACK, 0);
        set_signal(SIG_NUM_ECN, 0);
    }

    return PCM_SUCCESS;
}