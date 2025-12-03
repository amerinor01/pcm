#include "strack_light_v2.h"
#include "algo_utils.h"
#include "pcmh.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"

#define PATH_MASK (NO_OF_PATHS - 1)
BITMAP_HELPERS_DEFINE(VAR_ARR_EVS_BITMAP)

int algorithm_main() {
    if (!get_var_uint(VAR_IS_INITIALIZED)) {
        set_var_uint(VAR_PATH_RANDOM, RAND() & 0xFFFF);
        set_var_uint(VAR_PATH_XOR, RAND() & NO_OF_PATHS);
        set_var_uint(VAR_IS_INITIALIZED, 1);
    }

    pcm_uint trigger_mask = get_signal_trigger_mask();

    // Read currents
    pcm_uint cur_ack = get_signal(SIG_NUM_ACK);
    pcm_uint cur_ecn = get_signal(SIG_NUM_ECN);
    pcm_uint cur_nack = get_signal(SIG_NUM_NACK);
    pcm_uint cur_backlog = get_signal(SIG_TX_BACKLOG_SIZE);

    // RX path
    if (trigger_mask & SIG_NUM_ACK) {
        set_var_uint(VAR_PREV_ACK, cur_ack);
        BITMAP_HELPER_SET_ENTRY(VAR_ARR_EVS_BITMAP, get_signal(SIG_ACK_EV) & PATH_MASK, 0);
    }
    if (trigger_mask & SIG_NUM_ECN) {
        set_var_uint(VAR_PREV_ECN, cur_ecn);
        BITMAP_HELPER_SET_ENTRY(VAR_ARR_EVS_BITMAP, get_signal(SIG_ECN_EV) & PATH_MASK, 1);
    }
    if (trigger_mask & SIG_NUM_NACK) {
        set_var_uint(VAR_PREV_NACK, cur_nack);
        BITMAP_HELPER_SET_ENTRY(VAR_ARR_EVS_BITMAP, get_signal(SIG_NACK_EV) & PATH_MASK, 1);
    }

    // TX path
    if (trigger_mask & SIG_TX_BACKLOG_SIZE) {
        set_var_uint(VAR_PREV_BACKLOG, cur_backlog);

        pcm_uint ev = (get_var_uint(VAR_CUR_EV_IDX) ^ get_var_uint(VAR_PATH_XOR)) & PATH_MASK;
        if (!BITMAP_HELPER_GET_ENTRY(VAR_ARR_EVS_BITMAP, ev)) {
            set_var_uint(VAR_CUR_EV_IDX, get_var_uint(VAR_CUR_EV_IDX) + 1);
            if (get_var_uint(VAR_CUR_EV_IDX) == NO_OF_PATHS) {
                set_var_uint(VAR_CUR_EV_IDX, 0);
                set_var_uint(VAR_PATH_XOR, HASH(get_var_uint(VAR_PATH_XOR)) & PATH_MASK);
            }
        } else {
            pcm_uint rr_counter = 0;
            bool reset_flag = false;
            while (BITMAP_HELPER_GET_ENTRY(VAR_ARR_EVS_BITMAP, ev) && (rr_counter++ <= NO_OF_PATHS)) {
                if (!reset_flag) {
                    BITMAP_HELPER_SET_ENTRY(VAR_ARR_EVS_BITMAP, ev, 0);
                    reset_flag = true;
                }
                set_var_uint(VAR_CUR_EV_IDX, get_var_uint(VAR_CUR_EV_IDX) + 1);
                if (get_var_uint(VAR_CUR_EV_IDX) == NO_OF_PATHS) {
                    set_var_uint(VAR_CUR_EV_IDX, 0);
                    set_var_uint(VAR_PATH_XOR, HASH(get_var_uint(VAR_PATH_XOR)) & PATH_MASK);
                }
                ev = (get_var_uint(VAR_CUR_EV_IDX) ^ get_var_uint(VAR_PATH_XOR)) & PATH_MASK;
            }
        }
        ev |= get_var_uint(VAR_PATH_RANDOM) ^ (get_var_uint(VAR_PATH_RANDOM) & PATH_MASK);
        set_control(CTRL_NEXT_PKT_EV, ev);
    }

    return PCM_SUCCESS;
}
