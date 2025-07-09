#ifndef _DCQCN_H_
#define _DCQCN_H_

#include "fabric_params.h"
#include "pcm.h"

#define DCQCN_BYTE_COUNTER (2048 * 128)
#define DCQCN_RATE_INCREASE_TIMER (5000 * 1000)
#define DCQCN_GAMMA 0.00390625 /* 1/256 */
#define DCQCN_ALPHA_INIT 1.0
#define DCQCN_ALPHA_TIMER (10000 * 1000)
#define DCQCN_RAI 1
#define DCQCN_RHAI 10
#define DCQCN_FR_STEPS 5

struct dcqcn_state_snapshot {
    pcm_float alpha;
    pcm_float rate_cur;
    pcm_float rate_target;
    pcm_uint rate_increase_timer_evts;
    pcm_uint byte_counter_evts;
    pcm_uint rtt;
    pcm_uint cwnd;
};

enum dcqcn_signal_idxs {
    DCQCN_SIG_IDX_RTT = 0,
    DCQCN_SIG_IDX_ECN = 1,
    DCQCN_SIG_IDX_ALPHA_TIMER = 2,
    DCQCN_SIG_IDX_RATE_INCREASE_TIMER = 3,
    DCQCN_SIG_IDX_TX_BURST = 4
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