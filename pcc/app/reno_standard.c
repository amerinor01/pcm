#include <math.h>

#include "reno.h"

#define MAX(a, b)                                                              \
    ({                                                                         \
        __typeof__(a) _a = (a);                                                \
        __typeof__(b) _b = (b);                                                \
        _a > _b ? _a : _b;                                                     \
    })

/**
 * @brief Naive implementation of TCP Reno-like congestion window control.
 *
 * Compared to the standard event-based self-clocked Reno,
 * where cwnd/ssthresh are updated upon every received ACK/NACK/RTO,
 * this PCC-based implementation reacts upon the scheduler invokes Reno handler.
 * Flow state adjustment is done based on values observed in datapath signals,
 * where each signal accumulates ACK/NACK/RTO events.
 */
int algorithm_main() {
    /* 1) Read all incoming signals and current state */
    int num_nacks = get_signal(RENO_SIG_IDX_NACK);
    int num_rtos = get_signal(RENO_SIG_IDX_RTO);
    int num_acks = get_signal(RENO_SIG_IDX_ACK);
    int cwnd = get_control(RENO_CTRL_IDX_CWND);
    int ssthresh = get_local_state(RENO_LOCAL_STATE_IDX_SSTHRESH);
    int tot_acked = get_local_state(RENO_LOCAL_STATE_IDX_ACKED);
    int in_fast_recovery = get_local_state(RENO_LOCAL_STATE_IDX_IN_FAST_RECOV);

    /*-------------------------------------------------------------------------
     * 1) Fast Retransmit & Fast Recovery
     * On first duplicate‐ACK event, halve cwnd → ssthresh, then set
     * cwnd = ssthresh + 3 MSS and enter fast recovery.  On subsequent
     * duplicate ACKs, inflate cwnd by 1 MSS each.
     *------------------------------------------------------------------------*/
    if (num_nacks > 0) {
        if (!in_fast_recovery) {
            ssthresh = MAX(cwnd >> 1, 2);
            cwnd = ssthresh + 3;
            in_fast_recovery = 1;
            tot_acked = 0;
        } else {
            /* still in fast recovery, each additional duplicate ACK inflates
             * cwnd */
            cwnd += num_nacks;
        }
        set_signal(RENO_SIG_IDX_NACK, 0);
    }

    /*-------------------------------------------------------------------------
     * 2) Timeout Recovery (RTO)
     * On any RTO, unconditionally halve cwnd -> ssthresh, set cwnd = 1 MSS,
     * and exit fast recovery if we were in it.
     *------------------------------------------------------------------------*/
    if (num_rtos > 0) {
        ssthresh = MAX(cwnd >> 1, 2);
        cwnd = 1;
        in_fast_recovery = 0;
        tot_acked = 0;
        set_signal(RENO_SIG_IDX_RTO, 0);
    }

    /*-------------------------------------------------------------------------
     * 3) Detect exit from Fast Recovery
     * When in fast recovery, the first non‐duplicate ACK that acknowledges
     * new data (num_acks > 0) indicates recovery is complete.  At that point:
     *   - set cwnd = ssthresh
     *   - clear fast‐recovery flag
     *   - do NOT count this ACK toward standard increase
     *------------------------------------------------------------------------*/
    if (in_fast_recovery && num_acks > 0) {
        cwnd = ssthresh;
        in_fast_recovery = 0;
        num_acks = 0; /* suppress counting this ACK for slow start/CA */
        /* tot_acked is already 0 (from entry), so no need to clear again. */
    }

    /*-------------------------------------------------------------------------
     * Normal ACK Processing (slow start & congestion avoidance)
     * Only if we are not in fast recovery AND no RTO/nack signals remain:
     *   - In slow start (cwnd < ssthresh), cwnd += 1 MSS per ACK.
     *   - In congestion avoidance (cwnd >= ssthresh): 
     *      - increment by ~1 MSS per RTT: accumulate tot_acked += num_acks;
     *      - when tot_acked >= cwnd, do cwnd++,
     *      - subtract cwnd from tot_acked to preserve any “leftover”
     *credit.
     *------------------------------------------------------------------------*/
    if (!in_fast_recovery && num_rtos == 0 && num_nacks == 0 && num_acks > 0) {
        if (cwnd < ssthresh) {
            /* slow start */
            cwnd += num_acks;
        } else {
            /* congestion avoidance */
            tot_acked += num_acks;
            if (tot_acked >= cwnd) {
                tot_acked -= cwnd;
                cwnd += 1;
            }
        }
    }

    set_signal(RENO_SIG_IDX_ACK, 0);
    set_control(RENO_CTRL_IDX_CWND, cwnd);
    set_local_state(RENO_LOCAL_STATE_IDX_SSTHRESH, ssthresh);
    set_local_state(RENO_LOCAL_STATE_IDX_ACKED, tot_acked);
    set_local_state(RENO_LOCAL_STATE_IDX_IN_FAST_RECOV, in_fast_recovery);

    return SUCCESS;
}
