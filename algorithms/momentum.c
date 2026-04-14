#include "pcmh.h"
#include "algo_utils.h"
#include "pcm.h"
#include "momentum.h"

int algorithm_main() {
    pcm_uint initialized = get_var_uint(INIT);
    if (!initialized) {
        printf("MOMEMTUM: initializing handler\n");
        set_var_uint(LAST_ACK_TRIGGERED_TIME, get_signal(TIME_STAMP)); // for algorithm v1
        set_var_uint(INIT, 1);
    }
    pcm_uint trigger_mask = get_signal_trigger_mask();
    if (trigger_mask & NUM_NACK) {
        pcm_uint cur_cwnd =
            MAX((pcm_uint)MINIMUM_CWND_BYTES, get_control(CWND_BYTES) - get_signal(NUM_NACKED_BYTES));
        set_control(CWND_BYTES, cur_cwnd);
        printf("MOMEMTUM: newly_nacked_bytes=%llu\n", get_signal(NUM_NACKED_BYTES));
        update_signal(NUM_NACK, -get_signal(NUM_NACK));
        update_signal(NUM_NACKED_BYTES, -get_signal(NUM_NACKED_BYTES));
        return PCM_SUCCESS;
    } else {
        pcm_uint cur_cwnd = get_control(CWND_BYTES);
        printf("MOMEMTUM: curr_cwnd=%llu bytes_sent=%llu num_ecn=%llu num_ack=%llu ",
               cur_cwnd, get_signal(BYTES_SENT), get_signal(NUM_ECN), get_signal(NUM_ACK));
        pcm_uint num_ack = get_signal(NUM_ACK);
        pcm_uint num_ecn = get_signal(NUM_ECN);
        pcm_float ecn_portion = (pcm_float)num_ecn / num_ack;

        pcm_uint bytes_sent = get_signal(BYTES_SENT);
        // pcm_uint bytes_sent = get_var_uint(ACKED_BYTES);
        pcm_uint bytes_to_send = get_signal(BYTES_TO_SEND);
        pcm_float to_send_portion = (pcm_float)bytes_to_send / (bytes_sent + bytes_to_send);
        printf(" to_send_portion=%f", to_send_portion);
        pcm_float effective_gamma = GAMMA_MIN + (1 - POW(to_send_portion, GAMMA_POWER)) * (GAMMA_MAX - GAMMA_MIN);
        pcm_int dw = 0;
        if (!ALGO_V2) {
            pcm_uint time_stamp = get_signal(TIME_STAMP);
            pcm_uint dt_picosec  = time_stamp - get_var_uint(LAST_ACK_TRIGGERED_TIME);

            pcm_float dt = (pcm_float)(dt_picosec) / BRTT; // in units of base RTT
            pcm_float dw_dt = LOG_ALPHA * cur_cwnd;
            dw_dt -= ecn_portion * BETA;
            dw_dt += (1 - ecn_portion) * effective_gamma;
            dw = (pcm_int)(dw_dt * dt);
            printf(" dt=%f dw(exp)=%f dw(lin_dec)=%f dw(lin_recov)=%f total_dw=%lld\n",
                     dt,
                   LOG_ALPHA * cur_cwnd * dt,
                   - ecn_portion * BETA * dt,
                   (1 - ecn_portion) * effective_gamma * dt,
                   dw);
            set_var_uint(LAST_ACK_TRIGGERED_TIME, time_stamp);
        } else {
            // alternative representation
            pcm_uint acked_bytes = get_signal(NUM_ACKED_BYTES);
            pcm_float dw_f = LOG_ALPHA * acked_bytes; // exponential growth term
            dw_f -= ecn_portion * BETA / cur_cwnd * acked_bytes; // linear decrease term
            dw_f += (1 - ecn_portion) * effective_gamma / cur_cwnd * acked_bytes; // linear recovery term
            dw = (pcm_int)(dw_f);
            printf(" acked_bytes=%llu dw(exp)=%f dw(lin_dec)=%f dw(lin_recov)=%f total_dw=%lld\n",
                   acked_bytes,
                   LOG_ALPHA * acked_bytes,
                   - ecn_portion * BETA / cur_cwnd,
                   (1 - ecn_portion) * effective_gamma / cur_cwnd,
                   dw);
            set_signal(NUM_ACKED_BYTES, 0);
        }

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
        
        set_control(CWND_BYTES, MIN(MAX(cur_cwnd, MINIMUM_CWND_BYTES), MAXIMUM_CWND_BYTES));
        set_signal(NUM_ACK, 0);
        set_signal(NUM_ECN, 0);
    }

    return PCM_SUCCESS;
}