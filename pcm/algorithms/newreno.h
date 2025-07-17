#ifndef _NEWRENO_H_
#define _NEWRENO_H_

#include "pcm.h"

#define MSS 2048
#define CONST_GAMMA (0.00390625)

enum tcp_signal_idxs { SIG_NACK = 0, SIG_RTO = 1, SIG_ACK = 2, SIG_ECN = 3 };
enum tcp_control_idxs { CTRL_CWND = 0 };
enum tcp_local_var_idxs {
    VAR_SSTHRESH = 0,
    VAR_TOT_ACKED = 1,
    VAR_IN_FAST_RECOV = 2
};

#ifdef HANDLER_BUILD
int algorithm_main();

#else

#ifdef __cplusplus
extern "C" {
#endif

int __newreno_pcmc_init(pcm_handle_t new_handle);

#ifdef __cplusplus
}
#endif

#endif

#endif /* _NEWRENO_H_ */