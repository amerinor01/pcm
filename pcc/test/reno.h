#ifndef _RENO_H_
#define _RENO_H_

#include "pcm.h"

enum reno_signal_idxs {
    RENO_SIG_IDX_NACK = 0,
    RENO_SIG_IDX_RTO = 1,
    RENO_SIG_IDX_ACK = 2
};

enum reno_control_idxs { RENO_CTRL_IDX_CWND = 0 };

enum reno_local_var_idxs {
    RENO_LOCAL_STATE_IDX_SSTHRESH = 0,
    RENO_LOCAL_STATE_IDX_ACKED = 1
};

#ifdef HANDLER_BUILD
int algorithm_main();
#endif

#endif /* _RENO_H_ */