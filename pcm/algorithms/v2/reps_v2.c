#include "reps_v2.h"
#include "algo_utils.h"
#include "pcmh.h"
#include "stdlib.h"

#define PATH_MASK (EVS_SIZE - 1)
#define EVC_MASK (EVC_SIZE - 1)
BITMAP_HELPERS_DEFINE(VAR_EVC_BITMAP)

int algorithm_main() {
    if (!get_var_uint(VAR_IS_INITIALIZED)) {
        set_var_uint(VAR_EV_SEED, RAND() & PATH_MASK);
        set_var_uint(VAR_IS_INITIALIZED, 1);
    }

    pcm_uint trigger_mask = get_signal_trigger_mask();
    pcm_uint head_idx = get_var_uint(VAR_EVC_HEAD_IDX);
    pcm_uint num_valid_evs = get_var_uint(VAR_EVC_NUM_VALID_EVS);

    // Read Currents
    pcm_uint cur_ack = get_signal(SIG_NUM_ACK);
    pcm_uint cur_ecn = get_signal(SIG_NUM_ECN);
    pcm_uint cur_backlog = get_signal(SIG_TX_BACKLOG_SIZE);

    // Calculate Deltas
    pcm_uint delta_ack = cur_ack - get_var_uint(VAR_PREV_ACK);
    pcm_uint delta_ecn = cur_ecn - get_var_uint(VAR_PREV_ECN);
    pcm_uint delta_backlog = cur_backlog - get_var_uint(VAR_PREV_BACKLOG);

    bool processed = false; // sanity check

    if (trigger_mask & SIG_NUM_ACK) {
        set_var_uint(VAR_PREV_ACK, cur_ack);
        set_var_uint(VAR_PREV_ECN, cur_ecn);

        if (!delta_ecn) {
            set_arr_uint(VAR_EVC, head_idx, get_signal(SIG_ACK_EV));
            if (!BITMAP_HELPER_GET_ENTRY(VAR_EVC_BITMAP, head_idx)) {
                ++num_valid_evs;
                BITMAP_HELPER_SET_ENTRY(VAR_EVC_BITMAP, head_idx, 1);
            }
            head_idx = (head_idx + 1) & EVC_MASK;
        }
        processed = true;
    }

    if (trigger_mask & SIG_TX_BACKLOG_SIZE) {
        set_var_uint(VAR_PREV_BACKLOG, cur_backlog);

        pcm_uint packet_ev = 0;
        if (num_valid_evs == 0 || get_var_uint(VAR_EV_EXPLORE_COUNTER) > 0) {
            packet_ev = HASH(get_var_uint(VAR_EV_SEED));
            set_var_uint(VAR_EV_SEED, packet_ev);
            if (get_var_uint(VAR_EV_EXPLORE_COUNTER) > 0) {
                set_var_uint(VAR_EV_EXPLORE_COUNTER, get_var_uint(VAR_EV_EXPLORE_COUNTER) - 1);
            }
        } else {
            pcm_uint ev_cache_idx = (head_idx - num_valid_evs) & EVC_MASK;
            packet_ev = get_arr_uint(VAR_EVC, ev_cache_idx);
            BITMAP_HELPER_SET_ENTRY(VAR_EVC_BITMAP, ev_cache_idx, 0);
            --num_valid_evs;
        }
        set_control(CTRL_NEXT_PKT_EV, packet_ev);
        processed = true;
    }

    set_var_uint(VAR_EVC_HEAD_IDX, head_idx);
    set_var_uint(VAR_EVC_NUM_VALID_EVS, num_valid_evs);
    return processed ? PCM_SUCCESS : PCM_ERROR;
}
