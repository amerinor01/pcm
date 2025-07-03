#ifndef _SWIFT_H_
#define _SWIFT_H_

#include <math.h>
#include <stdbool.h>

#include "algo_utils.h"
#include "fabric_params.h"
#include "pcm.h"

#define SWIFT_RETX_RESET_THRESHOLD 4         /* as in S3.6 */
#define SWIFT_H (FABRIC_BASE_RTT / 6.55)     //  Value is a guess
#define SWIFT_FS_RANGE (5 * FABRIC_BASE_RTT) //  Value is a guess
#define SWIFT_FS_ALPHA                                                         \
    (SWIFT_FS_RANGE /                                                          \
     ((1.0 / sqrt(FABRIC_MIN_CWND)) - (1.0 / sqrt(FABRIC_MAX_CWND))))
#define SWIFT_FS_BETA -(SWIFT_FS_ALPHA / sqrt(FABRIC_MAX_CWND))
#define SWIFT_MAX_MDF 0.5 // max multiplicate decrease factor.  Value is a guess
#define SWIFT_AI 1.0      // increase constant.  Value is a guess
#define SWIFT_BETA 0.8    // decrease constant.  Value is a guess

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

int swift_pcmc_init(handle_t new_handle);

#ifdef __cplusplus
}
#endif

#endif

#endif /* _SWIFT_H_ */