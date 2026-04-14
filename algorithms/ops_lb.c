#include "ops_lb.h"
#include "assert.h"
#include "pcmh.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
#include "algo_utils.h"

#define PATH_MASK (NO_OF_PATHS - 1)

int algorithm_main() {
    if (!get_var_uint(IS_INITIALIZED)) {
        set_var_uint(PATH_RANDOM, rand() & 0xFFFF);
        set_var_uint(PATH_XOR, rand() % NO_OF_PATHS);
        set_var_uint(IS_INITIALIZED, 1);
    }

    assert(get_signal_trigger_mask() & TX_BACKLOG_SIZE);

    pcm_uint ev = (get_var_uint(CURRENT_EV_IDX) ^ get_var_uint(PATH_XOR)) & PATH_MASK;
    set_var_uint(CURRENT_EV_IDX, get_var_uint(CURRENT_EV_IDX) + 1);
    if (get_var_uint(CURRENT_EV_IDX) == NO_OF_PATHS) {
        set_var_uint(CURRENT_EV_IDX, 0);
        set_var_uint(PATH_XOR, HASH(get_var_uint(PATH_XOR)) & PATH_MASK);
    }

    ev |= get_var_uint(PATH_RANDOM) ^ (get_var_uint(PATH_RANDOM) & PATH_MASK);

    set_control(NEXT_PKT_EV, ev);
    update_signal(TX_BACKLOG_SIZE, -1);

    return PCM_SUCCESS;
}