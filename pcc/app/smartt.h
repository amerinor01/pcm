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

struct smartt_state_snapshot {
    int acked_bytes;     /* bytes acked since last QA */
    int bytes_ignored;   /* QA-ignore counter */
    int bytes_to_ignore; /* QA-ignore threshold */
    int trigger_qa;      /* QA active */
    int qa_deadline;     /* timestamp to limit QA frequency */
    int fast_count;      /* accumulator for fast increase */
    int fast_active;     /* fast increase mode active */
    int last_rtt;        /* last measured RTT in us */
    int avg_rtt;         /* average RTT in us */
    int now;             /* current timestamp */
    int cwnd;            /* congestion window in bytes */
    int num_acks;
    int num_nacks;
    int num_rtos;
    int num_ecns; // instead of last_pkt_ecn
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

#endif /* _SMARTT_H_ */