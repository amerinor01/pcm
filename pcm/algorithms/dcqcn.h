#ifndef _DCQCN_H_
#define _DCQCN_H_

#include "pcm.h"
#include "fabric_params.h"

#define DCQCN_BYTE_COUNTER (4096 * 2)
#define DCQCN_RATE_INCREASE_TIMER 100
#define DCQCN_GAMMA 0.00390625 /* 1/256 */
#define DCQCN_ALPHA_INIT 1.0
#define DCQCN_ALPHA_TIMER 55
#define DCQCN_RAI 1
#define DCQCN_RHAI 10
#define DCQCN_FR_STEPS 5

struct dcqcn_state_snapshot {
    float alpha;
    float rate_cur;
    float rate_target;
    int rate_increase_timer_evts;
    int byte_counter_evts;
    int rtt;
    int cwnd;
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