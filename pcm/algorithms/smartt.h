#ifndef _SMARTT_H_
#define _SMARTT_H_

#include "fabric_params.h"

/* smartt parameters */
#define SMARTT_TRTT_FACTOR 1.5 /* target RTT factor over base RTT */
#define SMARTT_TARGET_RTT (FABRIC_BASE_RTT * SMARTT_TRTT_FACTOR)
#define SMARTT_PI_CONST_COMP(brtt, trtt)                                       \
    ((brtt) / (trtt - brtt)) /* proportional increase */
#define SMARTT_PI_CONST SMARTT_PI_CONST_COMP(FABRIC_BASE_RTT, SMARTT_TARGET_RTT)
#define SMARTT_MD_CONST 0.8   /* multiplicative decrease constant */
#define SMARTT_FI_CONST 0.5   /* fair increase constant */
#define SMARTT_K_CONST 2      /* fast increase constant */
#define SMARTT_QA_SCALING 0.8 /* QuickAdapt scaling factor */
#define SMARTT_QA_DEADLINE (SMARTT_TRTT_FACTOR * FABRIC_BASE_RTT)
#define SMARTT_FI_TOL 1e-6

struct smartt_state_snapshot {
    pcm_uint acked_bytes;     /* bytes acked since last QA */
    pcm_uint bytes_ignored;   /* QA-ignore counter */
    pcm_uint bytes_to_ignore; /* QA-ignore threshold */
    pcm_uint trigger_qa;      /* QA active */
    pcm_uint qa_deadline;     /* timestamp to limit QA frequency */
    pcm_uint fast_count;      /* accumulator for fast increase */
    pcm_uint fast_active;     /* fast increase mode active */
    pcm_uint last_rtt;        /* last measured RTT in us */
    pcm_uint avg_rtt;         /* average RTT in us */
    pcm_uint now;             /* current timestamp */
    pcm_uint cwnd;            /* congestion window in bytes */
    pcm_uint num_acks;
    pcm_uint num_nacks;
    pcm_uint num_rtos;
    pcm_uint num_ecns; // instead of last_pkt_ecn
    struct smartt_constants {
        pcm_uint bdp;
        pcm_uint brtt;
        pcm_uint trtt;
        pcm_uint mss;
        pcm_uint reaction_delay;
        pcm_float y_gain;
        pcm_float x_gain;
        pcm_float z_gain;
        pcm_float w_gain;
        pcm_float qa_scaling;
    } consts;
};

enum smartt_local_state_idxs {
    SMARTT_LOCAL_STATE_ACKED_BYTES = 0,
    SMARTT_LOCAL_STATE_BYTES_IGNORED = 1,
    SMARTT_LOCAL_STATE_BYTES_TO_IGNORE = 2,
    SMARTT_LOCAL_STATE_TRIGGER_QA = 3,
    SMARTT_LOCAL_STATE_QA_DEADLINE = 4,
    SMARTT_LOCAL_STATE_FAST_COUNT = 5,
    SMARTT_LOCAL_STATE_FAST_ACTIVE = 6
};

enum smartt_signal_idxs {
    SMARTT_SIG_LAST_RTT = 0,
    SMARTT_SIG_NUM_ECN = 1,
    SMARTT_SIG_NUM_ACK = 2,
    SMARTT_SIG_NUM_RTO = 3,
    SMARTT_SIG_NUM_NACK = 4,
    SMARTT_SIG_ELAPSED_TIME = 5,
};

enum smartt_ctrl_idxs { SMARTT_CTRL_CWND_BYTES = 0 };

#ifdef HANDLER_BUILD
int algorithm_main();
#else

#ifdef __cplusplus
extern "C" {
#endif

int smartt_pcmc_init(handle_t new_handle);

#ifdef __cplusplus
}
#endif

#endif

#endif /* _SMARTT_H_ */