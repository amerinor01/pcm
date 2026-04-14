#include "reps_v2.h"
#include "algo_utils.h"
#include "pcmh.h"
#include "stdlib.h"

#define PATH_MASK (EVS_SIZE - 1)
#define EVC_MASK (EVC_SIZE - 1)
BITMAP_HELPERS_DEFINE(EVC_BITMAP)

int algorithm_main() {
    if (!get_var_uint(IS_INITIALIZED)) {
        set_var_uint(EV_SEED, RAND() & PATH_MASK);
        set_var_uint(IS_INITIALIZED, 1);
    }

    pcm_uint trigger_mask = get_signal_trigger_mask();
    pcm_uint head_idx = get_var_uint(EVC_HEAD_IDX);
    pcm_uint num_valid_evs = get_var_uint(EVC_NUM_VALID_EVS);

    // Read Currents
    pcm_uint cur_ecn = get_signal(NUM_ECN);

    // Calculate Deltas
    pcm_uint delta_ecn = cur_ecn - get_var_uint(PREV_ECN);

    bool processed = false; // sanity check

    if (trigger_mask & NUM_ACK) {
        set_var_uint(PREV_ECN, cur_ecn);

        if (!delta_ecn) {
            set_arr_uint(EVC, head_idx, get_signal(ACK_EV));
            if (!BITMAP_HELPER_GET_ENTRY(EVC_BITMAP, head_idx)) {
                ++num_valid_evs;
                BITMAP_HELPER_SET_ENTRY(EVC_BITMAP, head_idx, 1);
            }
            head_idx = (head_idx + 1) & EVC_MASK;
        }
        processed = true;
    }

    if (trigger_mask & TX_BACKLOG_SIZE) {
        pcm_uint packet_ev = 0;
        if (num_valid_evs == 0 || get_var_uint(EV_EXPLORE_COUNTER) > 0) {
            packet_ev = HASH(get_var_uint(EV_SEED)) & PATH_MASK;
            set_var_uint(EV_SEED, packet_ev);
            if (get_var_uint(EV_EXPLORE_COUNTER) > 0) {
                set_var_uint(EV_EXPLORE_COUNTER, get_var_uint(EV_EXPLORE_COUNTER) - 1);
            }
        } else {
            pcm_uint ev_cache_idx = (head_idx - num_valid_evs) & EVC_MASK;
            packet_ev = get_arr_uint(EVC, ev_cache_idx);
            BITMAP_HELPER_SET_ENTRY(EVC_BITMAP, ev_cache_idx, 0);
            --num_valid_evs;
        }
        set_control(NEXT_PKT_EV, packet_ev);
        processed = true;
    }

    set_var_uint(EVC_HEAD_IDX, head_idx);
    set_var_uint(EVC_NUM_VALID_EVS, num_valid_evs);
    return processed ? PCM_SUCCESS : PCM_ERROR;
}
