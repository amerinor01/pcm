#ifndef _SWIFT_H_
#define _SWIFT_H_

#include <math.h>
#include <stdbool.h>

#include "algo_utils.h"
#include "pcm.h"

#define RETX_RESET_THRESHOLD 4                /* as in S3.6 */
#define SWIFT_MIN_CWND 1                            /* in MSS units */
#define SWIFT_MAX_CWND 100                          /* some implementation cap */
#define SWIFT_BASE_DELAY 20.0                 //  Value is a guess
#define SWIFT_H (SWIFT_BASE_DELAY / 6.55)     //  Value is a guess
#define SWIFT_HOP_COUNT 5                     //  Value is a guess
#define SWIFT_FS_RANGE (5 * SWIFT_BASE_DELAY) //  Value is a guess
#define SWIFT_FS_ALPHA                                                         \
    (SWIFT_FS_RANGE /                                                          \
     ((1.0 / sqrt(SWIFT_MIN_CWND)) - (1.0 / sqrt(SWIFT_MAX_CWND))))
#define SWIFT_FS_BETA -SWIFT_FS_ALPHA / sqrt(SWIFT_MAX_CWND)
#define SWIFT_MAX_MDF 0.5 // max multiplicate decrease factor.  Value is a guess
#define SWIFT_AI 1.0      // increase constant.  Value is a guess
#define SWIFT_BETA 0.8    // decrease constant.  Value is a guess
#define SWIFT_MSS 4096    //  Value is a guess

enum swift_signal_idxs {
    SWIFT_SIG_IDX_NACK = 0,
    SWIFT_SIG_IDX_RTO = 1,
    SWIFT_SIG_IDX_ACK = 2,
    SWIFT_SIG_IDX_RTT = 3,
    SIWFT_SIG_IDX_ELAPSED_TIME = 4
};

enum swift_control_idxs { SWIFT_CTRL_IDX_CWND = 0 };

enum swift_local_var_idxs {
    SWIFT_LOCAL_STATE_IDX_ACKED = 0,
    SWIFT_LOCAL_STATE_IDX_T_LAST_DECREASE = 1,
    SWIFT_LOCAL_STATE_IDX_RETRANSMIT_CNT = 2,
    SWIFT_LOCAL_STATE_IDX_RTT_ESTIM = 3
};

struct swift_state_shapshot {
    int num_nacks;
    int num_rtos;
    int num_acks;
    int now;
    int rtt_estim;
    int delay;
    int t_last_decrease;
    int cwnd;
    int cwnd_prev;
    int retransmit_cnt;
    bool can_decrease;
};

#ifdef HANDLER_BUILD
int algorithm_main();
#endif

#endif /* _SWIFT_H_ */