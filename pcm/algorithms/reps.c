#include "pcmh.h"

// signals
#define SIG_NUM_ACK 0 // trigger w threshold=1, sum
#define SIG_NUM_SENDS 1 // trigger w threshold=1, sum
#define SIG_LAST_PKT_IS_ECN 2 // not trigger, last
#define SIG_LAST_PKT_EV 3 // not trigger, last

// controls 
#define CTRL_PKT_EV 0

// variables
#define VAR_EVC_HEAD_IDX 0
#define VAR_EVC_NUM_VALID_EVS 1
#define VAR_EV_EXPLORE_COUNTER 2

#define ARRAY_EVC 0
#define BITMAP_EVC 0

// constants 
#define EVS_SIZE 256

// for now we assume that handler keeps up with the arrival rate of TX/ACK packets
// we also don't handle path failure (which should be exposed as a datapath signal)
// and, therefore, don't support implement freezing mode
int algorithm_main() {
    pcm_uint trigger_mask = get_signal_trigger_mask();
    pcm_uint head_idx = get_var_uint(VAR_EVC_HEAD_IDX);
    pcm_uint num_valid_evs = get_var_uint(VAR_EVC_NUM_VALID_EVS);

    if (trigger_mask & SIG_NUM_ACK) {
        if (!get_signal(SIG_LAST_PKT_IS_ECN)) {
            set_array_entry_uint(ARRAY_EVC, head_idx, get_signal(SIG_LAST_PKT_EV));
            if (!get_bitmap_entry(BITMAP_EVC, head_idx)) {
                ++num_valid_evs;
                set_bitmap_entry(BITMAP_EVC, head_idx, 1);
            }
            head_idx = (head_idx + 1) % get_array_size(ARRAY_EVC);
        }
        update_signal(SIG_NUM_ACK, -1);
    }

    if (trigger_mask & SIG_NUM_SENDS) {
        pcm_uint packet_ev = 0;
        if (num_valid_evs == 0 || get_var_uint(VAR_EV_EXPLORE_COUNTER) > 0) {
            packet_ev = rand() % EVS_SIZE;
            set_var_uint(VAR_EV_EXPLORE_COUNTER, get_var_uint(VAR_EV_EXPLORE_COUNTER) - 1);
        } else {
            pcm_uint ev_cache_idx = (head_idx - num_valid_evs) % get_array_size(ARRAY_EVC);
            packet_ev = get_array_entry_uint(ARRAY_EVC, ev_cache_idx);
            set_bitmap_entry(BITMAP_EVC, ev_cache_idx, 0);
            --num_valid_evs;
        }
        set_control(CTRL_PKT_EV, packet_ev);
        update_signal(SIG_NUM_SENDS, -1);
    }

    set_var_uint(VAR_EVC_HEAD_IDX, head_idx);
    set_var_uint(VAR_EVC_NUM_VALID_EVS, num_valid_evs);
    return PCM_SUCCESS;
}