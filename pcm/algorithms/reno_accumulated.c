#include <math.h>

#include "tcp.h"

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
    /*
     * Below we read datapath signals and Reno-specific per-flow state.
     * For now pcc_flow_signal_read/write_ wrappers deal with
     * implementation-specific details of handling different signal
     * datatypes, e.g., LE/BE representation.
     *
     * Note 1:
     * In principle, the design here could be simpler
     * For example, the struct pcc_flow_ctx could containt two pointers,
     * e.g., 1) void **signals 2) void *user_data However, would it be safe?
     *
     * Note 2:
     * Another limitation here is that the overall contiguous user_data
     * region design prevents SPMD vectorization, because ideally we'd want
     * to have all variables belonging to different flows to be laid down as
     * SoA, not AoS.
     *
     * Note 3:
     * Nothing prevents user to access regions outside the user data buffer,
     * which is unsafe (unless we have memory protection).
     * Can we ensure safety here through static analysis?
     */
    pcm_uint num_nacks = get_signal(TCP_SIG_IDX_NACK);
    pcm_uint num_rtos = get_signal(TCP_SIG_IDX_RTO);
    pcm_uint num_acks = get_signal(TCP_SIG_IDX_ACK);

    pcm_uint mss = get_constant_uint(TCP_CONST_MSS);

    pcm_uint cwnd = get_control(TCP_CTRL_IDX_CWND) / mss;

    pcm_uint ssthresh = get_local_state(TCP_LOCAL_STATE_IDX_SSTHRESH);
    pcm_uint tot_acked = get_local_state(TCP_LOCAL_STATE_IDX_ACKED) + num_acks;

    pcm_uint num_acks_consumed = 0;

    /* 1) Fast retransmit: multiplicative decrease */
    // this is broken!
    if (num_nacks > 0) {
        ssthresh = MAX(cwnd >> num_nacks, 2);
        cwnd = ssthresh;
        tot_acked = 0;
        update_signal(TCP_SIG_IDX_NACK, -1);
    }

    /* 2) Timeout recovery */
    if (num_rtos > 0) {
        /* Only trigger if we have exited slow-start */
        if (cwnd > ssthresh) {
            ssthresh = MAX(cwnd >> 1, 2);
            cwnd = 1;
            tot_acked = 0;
        } else {
            num_rtos = 0;
        }
        update_signal(TCP_SIG_IDX_RTO, -1);
    }

    /*
     * Note:
     * We can't distinguish here if the reported ACKs came
     * before or after congestion signals that are handled above.
     * Thus, we are conservative here, and increase window only if
     * congestion signals weren't received at all or could
     * be ignored (subsequent RTOs after first RTO recovery).
     */
    if (num_nacks == 0 && num_rtos == 0 && tot_acked > 0) {
        num_acks_consumed = num_acks;
        /* 3) ACK processing in case there is no loss */
        /* 3.1) slow start: +1 MSS per ACK */
        if (cwnd < ssthresh) {
            pcm_uint ssremaining = ssthresh - cwnd;
            pcm_uint use = tot_acked < ssremaining ? tot_acked : ssremaining;
            cwnd += use;
            tot_acked -= use;
        }
        /* 3.2) congestion avoidance: +1 MSS per cwnd ACKs */
        /*
         * Note 1:
         * The code below is a closed form solution for
         * while (tot_acked >= cwnd && cwnd >= ssthresh) {
         *    cwnd++;
         *    tot_acked -= cwnd;
         * }
         *
         * Note 2:
         * Another approach here could be to not rely on acked counter
         * at all. In CA state, the original Reno increments CWND every
         * RTT, so here we can use time-based signals instead.
         */
        if (tot_acked >= cwnd && cwnd >= ssthresh) {
            pcm_uint C = cwnd;
            pcm_float b = (pcm_float)(2 * C + 1);
            pcm_float disc = sqrtf(b * b + 8.0 * (pcm_float)tot_acked);
            pcm_uint N = (pcm_uint)((disc - b) / 2.0);
            cwnd += N;
            tot_acked -= N * C + (N * (N + 1) / 2);
        }
    }

    update_signal(TCP_SIG_IDX_ACK, -num_acks_consumed);
    set_control(TCP_CTRL_IDX_CWND, cwnd / mss);
    set_local_state(TCP_LOCAL_STATE_IDX_ACKED, tot_acked);
    set_local_state(TCP_LOCAL_STATE_IDX_SSTHRESH, ssthresh);

    return SUCCESS;
}