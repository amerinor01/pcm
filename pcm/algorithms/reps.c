#include "reps.h"
#include "algo_utils.h"
#include "pcmh.h"
#include "stdlib.h"

#define PATH_MASK (EVS_SIZE - 1) // EVS_SIZE shall be power of two!!
#define EVC_MASK (EVC_SIZE - 1)  // EVC_SIZE shall be power of two
BITMAP_HELPERS_DEFINE(VAR_EVC_BITMAP)

// for now we assume that handler keeps up with the arrival rate of TX/ACK packets
// we also don't handle path failure (which should be exposed as a datapath signal)
// and, therefore, don't support implement freezing mode
int algorithm_main() {
    if (!get_var_uint(VAR_IS_INITIALIZED)) {
        set_var_uint(VAR_EV_SEED, RAND() & PATH_MASK);
        set_var_uint(VAR_IS_INITIALIZED, 1);
    }

    pcm_uint trigger_mask = get_signal_trigger_mask();
    pcm_uint head_idx = get_var_uint(VAR_EVC_HEAD_IDX);
    pcm_uint num_valid_evs = get_var_uint(VAR_EVC_NUM_VALID_EVS);

    if (trigger_mask & SIG_NUM_ACK) {
        if (!get_signal(SIG_NUM_ECN)) {
            set_arr_uint(VAR_EVC, head_idx, get_signal(SIG_ACK_EV));
            if (!BITMAP_HELPER_GET_ENTRY(VAR_EVC_BITMAP, head_idx)) {
                ++num_valid_evs;
                BITMAP_HELPER_SET_ENTRY(VAR_EVC_BITMAP, head_idx, 1);
            }
            head_idx = (head_idx + 1) & EVC_MASK;
        } else {
            set_signal(SIG_NUM_ECN, 0);
        }
        set_signal(SIG_NUM_ACK, 0);
    }

    if (trigger_mask & SIG_TX_BACKLOG_SIZE) {
        pcm_uint packet_ev = 0;
        if (num_valid_evs == 0 || get_var_uint(VAR_EV_EXPLORE_COUNTER) > 0) {
            packet_ev = HASH(get_var_uint(VAR_EV_SEED));
            set_var_uint(VAR_EV_SEED, packet_ev);
            set_var_uint(VAR_EV_EXPLORE_COUNTER, get_var_uint(VAR_EV_EXPLORE_COUNTER) - 1);
        } else {
            pcm_uint ev_cache_idx = (head_idx - num_valid_evs) & EVC_MASK;
            packet_ev = get_arr_uint(VAR_EVC, ev_cache_idx);
            BITMAP_HELPER_SET_ENTRY(VAR_EVC_BITMAP, ev_cache_idx, 0);
            --num_valid_evs;
        }
        set_control(CTRL_NEXT_PKT_EV, packet_ev);
        update_signal(SIG_TX_BACKLOG_SIZE, -1);
    }

    set_var_uint(VAR_EVC_HEAD_IDX, head_idx);
    set_var_uint(VAR_EVC_NUM_VALID_EVS, num_valid_evs);
    return PCM_SUCCESS;
}