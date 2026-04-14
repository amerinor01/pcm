#include "strack_light_v2.h"
#include "algo_utils.h"
#include "pcmh.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"

#define PATH_MASK (NO_OF_PATHS - 1)
BITMAP_HELPERS_DEFINE(ARR_EVS_BITMAP)

int algorithm_main() {
    if (!get_var_uint(IS_INITIALIZED)) {
        set_var_uint(PATH_RANDOM, RAND() & 0xFFFF);
        set_var_uint(PATH_XOR, RAND() & NO_OF_PATHS);
        set_var_uint(IS_INITIALIZED, 1);
    }

    pcm_uint trigger_mask = get_signal_trigger_mask();
    bool processed = false; // sanity check

    // Read currents
    pcm_uint cur_ack = get_signal(NUM_ACK);
    pcm_uint cur_ecn = get_signal(NUM_ECN);
    pcm_uint cur_nack = get_signal(NUM_NACK);
    pcm_uint cur_backlog = get_signal(TX_BACKLOG_SIZE);

    // RX path
    if (trigger_mask & NUM_ACK) {
        set_var_uint(PREV_ACK, cur_ack);
        BITMAP_HELPER_SET_ENTRY(ARR_EVS_BITMAP, get_signal(ACK_EV) & PATH_MASK, 0);
        processed = true;
    }
    if (trigger_mask & NUM_ECN) {
        set_var_uint(PREV_ECN, cur_ecn);
        BITMAP_HELPER_SET_ENTRY(ARR_EVS_BITMAP, get_signal(ECN_EV) & PATH_MASK, 1);
        processed = true;
    }
    if (trigger_mask & NUM_NACK) {
        set_var_uint(PREV_NACK, cur_nack);
        BITMAP_HELPER_SET_ENTRY(ARR_EVS_BITMAP, get_signal(NACK_EV) & PATH_MASK, 1);
        processed = true;
    }

    // TX path
    if (trigger_mask & TX_BACKLOG_SIZE) {
        set_var_uint(PREV_BACKLOG, cur_backlog);

        pcm_uint ev = (get_var_uint(CUR_EV_IDX) ^ get_var_uint(PATH_XOR)) & PATH_MASK;
        if (!BITMAP_HELPER_GET_ENTRY(ARR_EVS_BITMAP, ev)) {
            set_var_uint(CUR_EV_IDX, get_var_uint(CUR_EV_IDX) + 1);
            if (get_var_uint(CUR_EV_IDX) == NO_OF_PATHS) {
                set_var_uint(CUR_EV_IDX, 0);
                set_var_uint(PATH_XOR, HASH(get_var_uint(PATH_XOR)) & PATH_MASK);
            }
        } else {
            pcm_uint rr_counter = 0;
            bool reset_flag = false;
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
        processed = true;
    }

    return processed ? PCM_SUCCESS : PCM_ERROR;
}
