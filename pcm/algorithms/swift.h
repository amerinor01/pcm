#ifndef _SWIFT_H_
#define _SWIFT_H_

#include <math.h>
#include <stdbool.h>

#include "algo_utils.h"
#include "fabric_params.h"
#include "pcm.h"

#define MSS FABRIC_LINK_MSS
#define BRTT FABRIC_BRTT
#define BDP FABRIC_BDP
#define HOP_COUNT FABRIC_HOP_COUNT
#define RTX_RESET_THRESH 4         /* as in S3.6 */
#define H (FABRIC_BRTT / 6.55)     //  Value is a guess
#define FS_RANGE (5 * FABRIC_BRTT) //  Value is a guess
#define FS_ALPHA                                                               \
    (FS_RANGE / ((1.0 / sqrt(FABRIC_MIN_CWND)) - (1.0 / sqrt(FABRIC_MAX_CWND))))
#define FS_BETA -(FS_ALPHA / sqrt(FABRIC_MAX_CWND))
#define MAX_MDF 0.5 // max multiplicate decrease factor.  Value is a guess
#define AI 1.0      // increase constant.  Value is a guess
#define BETA 0.8    // decrease constant.  Value is a guess

enum swift_signal_idxs {
    SIG_NACK = 0,
    SIG_RTO = 1,
    SIG_ACK = 2,
    SIG_RTT = 3,
    SIG_ELAPSED_TIME = 4
};

enum swift_control_idxs { CTRL_CWND = 0 };

enum swift_local_var_idxs {
    VAR_ACKED = 0,
    VAR_T_LAST_DECREASE = 1,
    VAR_RTX_CNT = 2,
    VAR_RTT_ESTIM = 3
};

struct swift_state_snapshot {
    pcm_uint num_nacks;
    pcm_uint num_rtos;
    pcm_uint tot_acked;
    pcm_uint num_acks;
    pcm_uint now;
    pcm_uint rtt_estim;
    pcm_uint delay;
    pcm_uint t_last_decrease;
    pcm_uint cwnd;
    pcm_uint cwnd_prev;
    pcm_uint retransmit_cnt;
    bool can_decrease;
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