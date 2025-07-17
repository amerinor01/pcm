#ifndef _SWIFT_H_
#define _SWIFT_H_

#include "pcm.h"

#define BRTT 5058000
#define MSS 2048
#define FS_RANGE 25290000
#define FS_ALPHA 8231935.553788835
#define FS_BETA -741665.901692245
#define H 772213.7404580152
#define BETA 0.8
#define HOP_COUNT 6
#define RETX_RESET_THRESH 4
#define AI 1.0
#define MAX_MDF 0.5

enum swift_signals {
    SIG_NACK = 0,
    SIG_RTO = 1,
    SIG_ACK = 2,
    SIG_RTT = 3,
    SIG_ELAPSED_TIME = 4,
};

enum swift_controls {
    CTRL_CWND = 0,
};

enum swift_variables {
    VAR_ACKED = 0,
    VAR_T_LAST_DECREASE = 1,
    VAR_RETX_CNT = 2,
    VAR_RTT_ESTIM = 3,
};

#ifdef HANDLER_BUILD
int algorithm_main();
#else

#ifdef __cplusplus
extern "C" {
#endif

int __swift_pcmc_init(pcm_handle_t new_handle);

#ifdef __cplusplus
}
#endif

#endif
#endif /* _SWIFT_H_ */