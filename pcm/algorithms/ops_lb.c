#include "ops_lb.h"
#include "assert.h"
#include "pcmh.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"

#define PATH_MASK (NO_OF_PATHS - 1)

int algorithm_main() {
    printf("OPS HANDLER\n");
    if (!get_var_uint(VAR_IS_INITIALIZED)) {
        set_var_uint(VAR_PATH_RANDOM, rand() & 0xFFFF);
        set_var_uint(VAR_PATH_XOR, rand() % NO_OF_PATHS);
        set_var_uint(VAR_IS_INITIALIZED, 1);
    }

    assert(get_signal_trigger_mask() & SIG_TX_BACKLOG_SIZE);

    pcm_uint ev = (get_var_uint(VAR_CURRENT_EV_IDX) ^ get_var_uint(VAR_PATH_XOR)) & PATH_MASK;
    set_var_uint(VAR_CURRENT_EV_IDX, get_var_uint(VAR_CURRENT_EV_IDX) + 1);
    if (get_var_uint(VAR_CURRENT_EV_IDX) == NO_OF_PATHS) {
        set_var_uint(VAR_CURRENT_EV_IDX, 0);
        set_var_uint(VAR_PATH_XOR, rand() & PATH_MASK);
    }

    ev |= get_var_uint(VAR_PATH_RANDOM) ^ (get_var_uint(VAR_PATH_RANDOM) & PATH_MASK);

    set_control(CTRL_NEXT_PKT_EV, ev);
    update_signal(SIG_TX_BACKLOG_SIZE, -1);

    return PCM_SUCCESS;
}