#include "strack_light.h"
#include "algo_utils.h"
#include "pcmh.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"

#define PATH_MASK (NO_OF_PATHS - 1)
BITMAP_HELPERS_DEFINE(ARR_EVS_BITMAP)

// for now we assume that handler keeps up with the arrival rate of TX/ACK packets
// we also don't handle path failure (which should be exposed as a datapath signal)
// and, therefore, don't support implement freezing mode
int algorithm_main() {
    if (!get_var_uint(IS_INITIALIZED)) {
        set_var_uint(PATH_RANDOM, RAND() & 0xFFFF);
        set_var_uint(PATH_XOR, RAND() & NO_OF_PATHS);
        set_var_uint(IS_INITIALIZED, 1);
    }

    pcm_uint trigger_mask = get_signal_trigger_mask();
    bool processed = false; // sanity check

    // RX path
    if (trigger_mask & NUM_ACK) {
        BITMAP_HELPER_SET_ENTRY(ARR_EVS_BITMAP, get_signal(ACK_EV) & PATH_MASK, 0);
        set_signal(NUM_ACK, 0);
        processed = true;
    }
    if (trigger_mask & NUM_ECN) {
        BITMAP_HELPER_SET_ENTRY(ARR_EVS_BITMAP, get_signal(ECN_EV) & PATH_MASK, 1);
        set_signal(NUM_ECN, 0);
        processed = true;
    }
    if (trigger_mask & NUM_NACK) {
        BITMAP_HELPER_SET_ENTRY(ARR_EVS_BITMAP, get_signal(NACK_EV) & PATH_MASK, 1);
        set_signal(NUM_NACK, 0);
        processed = true;
    }

    // TX path
    if (trigger_mask & TX_BACKLOG_SIZE) {
        pcm_uint ev = (get_var_uint(CUR_EV_IDX) ^ get_var_uint(PATH_XOR)) & PATH_MASK;
        if (!BITMAP_HELPER_GET_ENTRY(ARR_EVS_BITMAP, ev)) {
            // EV is clean, so we just bump index
            set_var_uint(CUR_EV_IDX, get_var_uint(CUR_EV_IDX) + 1);
            if (get_var_uint(CUR_EV_IDX) == NO_OF_PATHS) {
                set_var_uint(CUR_EV_IDX, 0);
                set_var_uint(PATH_XOR, HASH(get_var_uint(PATH_XOR)) & PATH_MASK);
            }
        } else {
            pcm_uint rr_counter = 0; // Do round robin across EVs
            bool reset_flag = false; // One packet can consume only one EV
            while (BITMAP_HELPER_GET_ENTRY(ARR_EVS_BITMAP, ev) && (rr_counter++ <= NO_OF_PATHS)) {
                if (!reset_flag) {
                    BITMAP_HELPER_SET_ENTRY(ARR_EVS_BITMAP, ev, 0);
                    reset_flag = true;
                }
                set_var_uint(CUR_EV_IDX, get_var_uint(CUR_EV_IDX) + 1);
                if (get_var_uint(CUR_EV_IDX) == NO_OF_PATHS) {
                    set_var_uint(CUR_EV_IDX, 0);
                    set_var_uint(PATH_XOR, HASH(get_var_uint(PATH_XOR)) & PATH_MASK);
                }
                ev = (get_var_uint(CUR_EV_IDX) ^ get_var_uint(PATH_XOR)) & PATH_MASK;
            }
        }
        ev |= get_var_uint(PATH_RANDOM) ^ (get_var_uint(PATH_RANDOM) & PATH_MASK);
        set_control(NEXT_PKT_EV, ev);
        update_signal(TX_BACKLOG_SIZE, -1);
        processed = true;
    }

    return processed ? PCM_SUCCESS : PCM_ERROR;
}