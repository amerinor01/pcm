#include "uec_dctcp.h"
#include "algo_utils.h"
#include "pcm.h"
#include "pcmh.h"

int algorithm_main() {
    pcm_uint trigger_mask = get_signal_trigger_mask();
    if (trigger_mask & SIG_NUM_NACKED_BYTES) {
        pcm_uint cur_cwnd = get_control(CTRL_CWND_BYTES);
        if (cur_cwnd > get_signal(SIG_NUM_NACKED_BYTES))
            cur_cwnd = MAX((pcm_uint)MSS, get_control(CTRL_CWND_BYTES) - get_signal(SIG_NUM_NACKED_BYTES));
        set_control(CTRL_CWND_BYTES, cur_cwnd);
        update_signal(SIG_NUM_NACKED_BYTES, -get_signal(SIG_NUM_NACKED_BYTES));
        return PCM_SUCCESS;
    } else if (trigger_mask & SIG_NUM_ACKED_BYTES) {
        pcm_uint cur_cwnd = get_control(CTRL_CWND_BYTES);
        pcm_uint newly_acked_bytes = get_signal(SIG_NUM_ACKED_BYTES);
        if (!get_signal(SIG_NUM_ECN)) {
            cur_cwnd += newly_acked_bytes * MSS / cur_cwnd;
            cur_cwnd = MAX((pcm_uint)MSS, cur_cwnd);
        } else {
            cur_cwnd -= newly_acked_bytes / 3;
            update_signal(SIG_NUM_ECN, -get_signal(SIG_NUM_ECN));
        }
        update_signal(SIG_NUM_ACKED_BYTES, -get_signal(SIG_NUM_ACKED_BYTES));
        set_control(CTRL_CWND_BYTES, cur_cwnd);
    }

    return PCM_SUCCESS;
}