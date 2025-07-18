#ifndef _SMARTT_H_
#define _SMARTT_H_

#include "fabric_params.h"

/* smartt parameters */
#define CONST_FI_BRTT_TOL 0.75
#define CONST_BDP FABRIC_BDP
#define CONST_BRTT FABRIC_BRTT
#define CONST_TRTT FABRIC_TRTT
#define CONST_MSS FABRIC_LINK_MSS
#define CONST_REACTION_DELAY 1.0
#define CONST_X_GAIN 2.0
#define CONST_Y_GAIN 2.5
#define CONST_Z_GAIN 2
#define CONST_W_GAIN 0.8
#define CONST_QA_SCALING 1

enum smartt_var_idxs {
    VAR_ACKED_BYTES = 0,
    VAR_BYTES_IGNORED = 1,
    VAR_BYTES_TO_IGNORE = 2,
    VAR_TRIGGER_QA = 3,
    VAR_QA_DEADLINE = 4,
    VAR_FAST_COUNT = 5,
    VAR_FAST_ACTIVE = 6
};

enum smartt_signal_idxs {
    SIG_RTT_SAMPLE = 0,
    SIG_NUM_ECN = 1,
    SIG_NUM_ACK = 2,
    SIG_NUM_RTO = 3,
    SIG_NUM_NACK = 4,
    SIG_ELAPSED_TIME = 5,
};

enum smartt_ctrl_idxs { CTRL_CWND_BYTES = 0 };

#ifdef HANDLER_BUILD
int algorithm_main();
#else

#ifdef __cplusplus
extern "C" {
#endif

int __smartt_pcmc_init(pcm_handle_t new_handle);

#ifdef __cplusplus
}
#endif

#endif

#endif /* _SMARTT_H_ */