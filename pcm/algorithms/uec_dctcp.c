#include "pcmh.h"
#include "uec_dctcp.h"
#include "algo_utils.h"
#include "pcm.h"

int algorithm_main() {
    pcm_uint trigger_mask = get_signal_trigger_mask();
    if (trigger_mask & SIG_NUM_NACK) {
        pcm_uint cur_cwnd =
            MAX(MSS, get_control(CTRL_CWND_BYTES) - get_signal(SIG_NUM_NACKED_BYTES));
        set_control(CTRL_CWND_BYTES, cur_cwnd);
        // printf("DCTCP: newly_nacked_bytes=%llu\n", get_signal(SIG_NUM_NACKED_BYTES),
        // get_signal(SIG_NUM_NACK) * MSS);
        update_signal(SIG_NUM_NACK, -get_signal(SIG_NUM_NACK));
        update_signal(SIG_NUM_NACKED_BYTES, -get_signal(SIG_NUM_NACKED_BYTES));
        return PCM_SUCCESS;
    } else if (trigger_mask & SIG_NUM_ACK) {
        pcm_uint cur_cwnd = get_control(CTRL_CWND_BYTES);
        pcm_uint newly_acked_bytes = get_signal(SIG_NUM_ACKED_BYTES);
        // printf("DCTCP: newly_acked_bytes=%llu num_ecns=%llu\n", newly_acked_bytes,
        // get_signal(SIG_NUM_ECN));
        if (!get_signal(SIG_NUM_ECN)) {
            cur_cwnd += newly_acked_bytes * MSS / cur_cwnd;
            cur_cwnd = MAX(MSS, cur_cwnd);
        } else {
            cur_cwnd -= newly_acked_bytes / 3;
            update_signal(SIG_NUM_ECN, -get_signal(SIG_NUM_ECN));
        }
        update_signal(SIG_NUM_ACKED_BYTES, -get_signal(SIG_NUM_ACKED_BYTES));
        update_signal(SIG_NUM_ACK, -get_signal(SIG_NUM_ACK));
        set_control(CTRL_CWND_BYTES, cur_cwnd);
    }

    return PCM_SUCCESS;
}