#include "reps.h"
#include "algo_utils.h"
#include "pcmh.h"
#include "stdlib.h"

#define PATH_MASK (EVS_SIZE - 1) // EVS_SIZE shall be power of two!!
#define EVC_MASK (EVC_SIZE - 1)  // EVC_SIZE shall be power of two
BITMAP_HELPERS_DEFINE(EVC_BITMAP)

// for now we assume that handler keeps up with the arrival rate of TX/ACK packets
// we also don't handle path failure (which should be exposed as a datapath signal)
// and, therefore, don't support implement freezing mode
int algorithm_main() {
    if (!get_var_uint(IS_INITIALIZED)) {
        set_var_uint(EV_SEED, RAND() & PATH_MASK);
        set_var_uint(IS_INITIALIZED, 1);
    }

    pcm_uint trigger_mask = get_signal_trigger_mask();
    pcm_uint head_idx = get_var_uint(EVC_HEAD_IDX);
    pcm_uint num_valid_evs = get_var_uint(EVC_NUM_VALID_EVS);
    bool processed = false; // sanity check

    if (trigger_mask & NUM_ACK) {
        if (!get_signal(NUM_ECN)) {
            set_arr_uint(EVC, head_idx, get_signal(ACK_EV));
            if (!BITMAP_HELPER_GET_ENTRY(EVC_BITMAP, head_idx)) {
                ++num_valid_evs;
                BITMAP_HELPER_SET_ENTRY(EVC_BITMAP, head_idx, 1);
            }
            head_idx = (head_idx + 1) & EVC_MASK;
        } else {
            // just discard bad EV
            set_signal(NUM_ECN, 0);
        }
        set_signal(NUM_ACK, 0);
        processed = true;
    }

    if (trigger_mask & TX_BACKLOG_SIZE) {
        pcm_uint packet_ev = 0;
        if (num_valid_evs == 0 || get_var_uint(EV_EXPLORE_COUNTER) > 0) {
            packet_ev = HASH(get_var_uint(EV_SEED)) & PATH_MASK;
            set_var_uint(EV_SEED, packet_ev);
            set_var_uint(EV_EXPLORE_COUNTER, get_var_uint(EV_EXPLORE_COUNTER) - 1);
        } else {
            pcm_uint ev_cache_idx = (head_idx - num_valid_evs) & EVC_MASK;
            packet_ev = get_arr_uint(EVC, ev_cache_idx);
            BITMAP_HELPER_SET_ENTRY(EVC_BITMAP, ev_cache_idx, 0);
            --num_valid_evs;
        }
        set_control(NEXT_PKT_EV, packet_ev);
        update_signal(TX_BACKLOG_SIZE, -1);
        processed = true;
    }

    set_var_uint(EVC_HEAD_IDX, head_idx);
    set_var_uint(EVC_NUM_VALID_EVS, num_valid_evs);
    return processed ? PCM_SUCCESS : PCM_ERROR;
}