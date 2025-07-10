#ifndef _DCQCN_H_
#define _DCQCN_H_

#include "fabric_params.h"
#include "pcm.h"

#define DEFAULT_DCQCN_BYTE_COUNTER (2048 * 128)
#define DEFAULT_DCQCN_RATE_INCREASE_TIMER (5000 * 1000)
#define DEFAULT_DCQCN_ALPHA_INIT 1.0
#define DEFAULT_DCQCN_ALPHA_TIMER (10000 * 1000)
#define DEFAULT_DCQCN_RAI 1
#define DEFAULT_DCQCN_RHAI 10
#define DEFAULT_DCQCN_FR_STEPS 5
#define DEFAULT_DCQCN_GAMMA 0.00390625 /* 1/256 */

enum dcqcn_signal_idxs {
    DCQCN_SIG_IDX_ECN = 0,
    DCQCN_SIG_IDX_ALPHA_TIMER = 1,
    DCQCN_SIG_IDX_RATE_INCREASE_TIMER = 2,
    DCQCN_SIG_IDX_TX_BURST = 3
};

enum dcqcn_local_var_idxs {
    DCQCN_LOCAL_STATE_IDX_ALPHA = 0,
    DCQCN_LOCAL_STATE_IDX_RATE_CUR = 1,
    DCQCN_LOCAL_STATE_IDX_RATE_TARGET = 2,
    DCQCN_LOCAL_STATE_IDX_RATE_INCREASE_EVTS = 3,
    DCQCN_LOCAL_STATE_IDX_BYTE_COUNTER_EVTS = 4
};

enum dcqcn_ctrl_idxs {
    DCQCN_CTRL_IDX_CWND = 0,
};

enum dcqcn_consts_idxs {
    DCQCN_CONST_BRTT = 0,
    DCQCN_CONST_RAI = 1,
    DCQCN_CONST_RHAI = 2,
    DCQCN_CONST_FR_STEPS = 3,
    DCQCN_CONST_GAMMA = 4,
};

struct dcqcn_constants {
    pcm_uint brtt;
    pcm_uint rai;
    pcm_uint rhai;
    pcm_uint fr_steps;
    pcm_float gamma;
};

struct dcqcn_state_snapshot {
    pcm_float alpha;
    pcm_float rate_cur;
    pcm_float rate_target;
    pcm_uint rate_increase_timer_evts;
    pcm_uint byte_counter_evts;
    pcm_uint cwnd;
    struct dcqcn_constants consts;
};

#ifdef HANDLER_BUILD
int algorithm_main();
#else

#ifdef __cplusplus
extern "C" {
#endif

int dcqcn_pcmc_init(handle_t new_handle);

#ifdef __cplusplus
}
#endif

#endif

#endif /* _DCQCN_H_ */