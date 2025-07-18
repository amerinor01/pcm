#ifndef _DCQCN_H_
#define _DCQCN_H_

#include "fabric_params.h"
#include "pcm.h"

#define DEFAULT_DCQCN_BYTE_COUNTER (2048 * 128)
#define DEFAULT_DCQCN_RATE_INCREASE_TIMER (5000 * 1000)
#define DEFAULT_DCQCN_ALPHA_INIT 1.0
#define DEFAULT_DCQCN_ALPHA_TIMER (10000 * 1000)

#define CONST_BRTT 5058000
#define CONST_RAI 1
#define CONST_RHAI 10
#define CONST_FR_STEPS 5
#define CONST_GAMMA 0.00390625 /* 1/256 */

enum dcqcn_signal_idxs {
    SIG_ECN = 0,
    SIG_ALPHA_TIMER = 1,
    SIG_RATE_INCREASE_TIMER = 2,
    SIG_TX_BURST = 3
};

enum dcqcn_local_var_idxs {
    VAR_ALPHA = 0,
    VAR_CUR_RATE = 1,
    VAR_TGT_RATE = 2,
    VAR_RATE_INCREASE_EVTS = 3,
    VAR_BYTE_COUNTER_EVTS = 4
};

enum dcqcn_ctrl_idxs {
    CTRL_CWND = 0,
};

#ifdef HANDLER_BUILD
int algorithm_main();
#else

#ifdef __cplusplus
extern "C" {
#endif

int __dcqcn_pcmc_init(pcm_handle_t new_handle);

#ifdef __cplusplus
}
#endif

#endif

#endif /* _DCQCN_H_ */