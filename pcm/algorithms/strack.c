#include "strack.h"
#include "algo_utils.h"
#include "pcmh.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"

#define PATH_MASK (NO_OF_PATHS - 1)

// for now we assume that handler keeps up with the arrival rate of TX/ACK packets
// we also don't handle path failure (which should be exposed as a datapath signal)
// and, therefore, don't support implement freezing mode
int algorithm_main() {
    if (!get_var_uint(VAR_IS_INITIALIZED)) {
        set_var_uint(VAR_PATH_RANDOM, RAND() & 0xFFFF);
        set_var_uint(VAR_PATH_XOR, RAND() & NO_OF_PATHS);
        set_var_uint(VAR_IS_INITIALIZED, 1);
    }

    pcm_uint trigger_mask = get_signal_trigger_mask();
    bool processed = false; // sanity check

    // RX path
    if (trigger_mask & SIG_NUM_ECN) {
        pcm_uint penalty = 1;
        pcm_uint ev = get_signal(SIG_ECN_EV) & PATH_MASK;
        pcm_uint new_penalty = (get_arr_uint(VAR_ARR_EVS_BITMAP, ev) + penalty) & MAX_PENALTY;
        set_arr_uint(VAR_ARR_EVS_BITMAP, ev, new_penalty);
        set_signal(SIG_NUM_ECN, 0);
        processed = true;
    }
    if (trigger_mask & SIG_NUM_NACK) {
        pcm_uint penalty = 4;
        pcm_uint ev = get_signal(SIG_NACK_EV) & PATH_MASK;
        pcm_uint new_penalty = (get_arr_uint(VAR_ARR_EVS_BITMAP, ev) + penalty) & MAX_PENALTY;
        set_arr_uint(VAR_ARR_EVS_BITMAP, ev, new_penalty);
        set_signal(SIG_NUM_NACK, 0);
        processed = true;
    }
    // TODO: add RTO

    // TX path
    if (trigger_mask & SIG_TX_BACKLOG_SIZE) {
        pcm_uint ev = (get_var_uint(VAR_CUR_EV_IDX) ^ get_var_uint(VAR_PATH_XOR)) & PATH_MASK;
        if (!get_arr_uint(VAR_ARR_EVS_BITMAP, ev)) {
            // EV is clean, so we just bump index
            set_var_uint(VAR_CUR_EV_IDX, get_var_uint(VAR_CUR_EV_IDX) + 1);
            if (get_var_uint(VAR_CUR_EV_IDX) == NO_OF_PATHS) {
                set_var_uint(VAR_CUR_EV_IDX, 0);
                set_var_uint(VAR_PATH_XOR, HASH(get_var_uint(VAR_PATH_XOR)) & PATH_MASK);
            }
        } else {
            pcm_uint rr_counter = 0; // Do round robin across EVs
            bool reset_flag = false; // One packet can consume only one EV
            while (get_arr_uint(VAR_ARR_EVS_BITMAP, ev) && (rr_counter++ <= NO_OF_PATHS)) {
                if (!reset_flag) {
                    set_arr_uint(VAR_ARR_EVS_BITMAP, ev, get_arr_uint(VAR_ARR_EVS_BITMAP, ev) - 1);
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
        update_signal(SIG_TX_BACKLOG_SIZE, -1);
        processed = true;
    }

    return processed ? PCM_SUCCESS : PCM_ERROR;
}