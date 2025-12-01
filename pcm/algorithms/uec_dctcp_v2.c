#include "uec_dctcp_v2.h"
#include "algo_utils.h"
#include "pcm.h"
#include "pcmh.h"

int algorithm_main() {
    pcm_uint cur_cwnd = get_control(CTRL_CWND_BYTES);
    pcm_uint trigger_mask = get_signal_trigger_mask();
    if (trigger_mask & SIG_NUM_NACKED_BYTES) {
        pcm_uint cur_nacked_bytes = get_signal(SIG_NUM_NACKED_BYTES) - get_var_uint(VAR_PREV_NACKED_BYTES);
        if (cur_cwnd > cur_nacked_bytes)
            cur_cwnd = MAX((pcm_uint)MSS, get_control(CTRL_CWND_BYTES) - cur_nacked_bytes);
        set_var_uint(VAR_PREV_NACKED_BYTES, get_signal(SIG_NUM_NACKED_BYTES));
    } else if (trigger_mask & SIG_NUM_ACKED_BYTES) {
        pcm_uint newly_acked_bytes = get_signal(SIG_NUM_ACKED_BYTES) - get_var_uint(VAR_PREV_ACKED_BYTES);
        pcm_uint newly_ecned_pkts = get_signal(SIG_NUM_ECN) - get_var_uint(VAR_PREV_ECNED_PKTS);
        if (!newly_ecned_pkts) {
            cur_cwnd += newly_acked_bytes * MSS / cur_cwnd;
            cur_cwnd = MAX((pcm_uint)MSS, cur_cwnd);
        } else if (cur_cwnd > newly_acked_bytes) {
            cur_cwnd -= newly_acked_bytes / 3;
        }
        set_var_uint(VAR_PREV_ACKED_BYTES, get_signal(SIG_NUM_ACKED_BYTES));
        set_var_uint(VAR_PREV_ECNED_PKTS, get_signal(SIG_NUM_ECN));
    }
    set_control(CTRL_CWND_BYTES, cur_cwnd);
    return PCM_SUCCESS;
}