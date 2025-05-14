#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "pcc.h"

#define MAX(a, b)                                                              \
    ({                                                                         \
        __typeof__(a) _a = (a);                                                \
        __typeof__(b) _b = (b);                                                \
        _a > _b ? _a : _b;                                                     \
    })

#define RENO_NUM_SIGNALS 4
#define RENO_PCC_SIGNALS                                                       \
    (PCC_SIG_TYPE_CWND | PCC_SIG_TYPE_ACKS_RECVD | PCC_SIG_TYPE_NACKS_RECVD |  \
     PCC_SIG_TYPE_RTOS)

enum reno_signal_idxs {
    RENO_SIGNAL_IDX_NACKS = 0,
    RENO_SIGNAL_IDX_RTOS = 1,
    RENO_SIGNAL_IDX_ACKS = 2,
    RENO_SIGNAL_IDX_CWND = 3,
};

struct reno_flow_ctx {
    uint64_t ssthresh; /**< current slow-start threshold */
    uint64_t acked;    /**< accumulated acks since last cwnd bump */
};

const struct reno_flow_ctx initial_reno_flow_values = {.acked = 0,
                                                       .ssthresh = UINT64_MAX};

/**
 * @brief Naive implementation of TCP Reno-like congestion window control.
 *
 * Compared to the standard event-based self-clocked Reno,
 * where cwnd/ssthresh are updated upon every received ACK/NACK/RTO,
 * this PCC-based implementation reacts upon the scheduler invokes Reno handler.
 * Flow state adjustment is done based on values observed in datapath signals,
 * where each signal accumulates ACK/NACK/RTO events.
 */
static int reno_algo_handler(struct pcc_flow_ctx *ctx) {
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
    uint64_t num_nacks = pcc_flow_signal_read_u64(ctx, RENO_SIGNAL_IDX_NACKS);
    uint64_t num_rtos = pcc_flow_signal_read_u64(ctx, RENO_SIGNAL_IDX_RTOS);
    uint64_t num_acks = pcc_flow_signal_read_u64(ctx, RENO_SIGNAL_IDX_ACKS);
    uint64_t cwnd = pcc_flow_signal_read_u64(ctx, RENO_SIGNAL_IDX_CWND);
    struct reno_flow_ctx *fs =
        (struct reno_flow_ctx *)pcc_flow_user_data_ptr_get(ctx);
    uint64_t ssthresh = fs->ssthresh;
    uint64_t tot_acked = fs->acked + num_acks;

    /* 1) Fast retransmit: multiplicative decrease */
    if (num_nacks > 0) {
        ssthresh = MAX(cwnd >> num_nacks, 2);
        cwnd = ssthresh;
        tot_acked = 0;
        pcc_flow_signal_write_u64(ctx, RENO_SIGNAL_IDX_NACKS, 0);
    }

    /* 2) Timeout recovery */
    if (num_rtos > 0) {
        /* Only trigger if we have exited slow-start */
        if (cwnd > ssthresh) {
            ssthresh = MAX(cwnd >> 1, 2);
            cwnd = ssthresh;
            tot_acked = 0;
        } else {
            num_rtos = 0;
        }
        pcc_flow_signal_write_u64(ctx, RENO_SIGNAL_IDX_RTOS, 0);
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
        /* 3) ACK processing in case there is no loss */
        /* 3.1) Slow start: grow cwnd up to ssthresh */
        if (cwnd < ssthresh) {
            uint64_t ssremaining = ssthresh - cwnd;
            uint64_t use = tot_acked < ssremaining ? tot_acked : ssremaining;
            cwnd += use;
            tot_acked -= use;
        }
        /* 3.2) Congestion avoidance: +1 MSS per cwnd ACKs */
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
            uint64_t C = cwnd;
            double b = (double)(2 * C + 1);
            double disc = sqrt(b * b + 8.0 * (double)tot_acked);
            uint64_t N = (uint64_t)((disc - b) / 2.0);
            cwnd += N;
            tot_acked -= N * C + (N * (N + 1) / 2);
        }
    }

    pcc_flow_signal_write_u64(ctx, RENO_SIGNAL_IDX_ACKS, 0);
    pcc_flow_signal_write_u64(ctx, RENO_SIGNAL_IDX_CWND, cwnd);
    fs->acked = tot_acked;
    fs->ssthresh = ssthresh;

    return PCC_SUCCESS;
}

int reno_algo_init(struct pcc_dev_ctx *dev_ctx,
                   struct pcc_algorithm_handler_obj **algo_handler) {
    /* Query the PCC capabilities supported */
    struct pcc_algorithm_handler_attrs *caps;
    if (pcc_dev_ctx_caps_query(dev_ctx, &caps)) {
        fprintf(stderr, "Failed to query device caps\n");
        return 1;
    }

    /* Check that capabilities are sufficient to enable Reno */
    if ((caps->signals_mask & RENO_PCC_SIGNALS) != RENO_PCC_SIGNALS) {
        fprintf(stderr, "PCC device doesn't support required signals\n");
        return 1;
    }

    /*
     * TODO: validate compatibility of every signal in caps versus Reno
     * requirements for (size_t sig_idx = 0; sig_idx < caps->num_signals;
     * sig_idx++) { if (caps->signal_attrs[sig_idx].type ==
     * PCC_SIG_TYPE_CWND) { } else if (caps->signal_attrs[sig_idx].type ==
     * PCC_SIG_TYPE_ACKS_RECVD) { } else if
     * (caps->signal_attrs[sig_idx].type == PCC_SIG_TYPE_NACKS_RECVD) { }
     * else if (caps->signal_attrs[sig_idx].type == PCC_SIG_TYPE_RTOS) {
     *   }
     * }
     */

    struct pcc_signal_attr signal_attrs[RENO_NUM_SIGNALS] = {
        [RENO_SIGNAL_IDX_CWND] = {.type = PCC_SIG_TYPE_CWND,
                                  .coalescing_op = PCC_SIG_COALESCING_OP_LAST,
                                  .reset_type = PCC_SIG_RESET_TYPE_USR,
                                  .dev_perms = PCC_SIG_READ,
                                  .user_perms = PCC_SIG_READ | PCC_SIG_WRITE,
                                  .dtype = PCC_SIG_DTYPE_UINT64,
                                  .init_value = 1},
        [RENO_SIGNAL_IDX_ACKS] = {.type = PCC_SIG_TYPE_ACKS_RECVD,
                                  .coalescing_op = PCC_SIG_COALESCING_OP_ACCUM,
                                  .reset_type = PCC_SIG_RESET_TYPE_USR,
                                  .dev_perms = PCC_SIG_WRITE,
                                  .user_perms = PCC_SIG_READ | PCC_SIG_WRITE,
                                  .dtype = PCC_SIG_DTYPE_UINT64,
                                  .init_value = 0},
        [RENO_SIGNAL_IDX_NACKS] = {.type = PCC_SIG_TYPE_NACKS_RECVD,
                                   .coalescing_op = PCC_SIG_COALESCING_OP_ACCUM,
                                   .reset_type = PCC_SIG_RESET_TYPE_USR,
                                   .dev_perms = PCC_SIG_WRITE,
                                   .user_perms = PCC_SIG_READ | PCC_SIG_WRITE,
                                   .dtype = PCC_SIG_DTYPE_UINT64,
                                   .init_value = 0},
        [RENO_SIGNAL_IDX_RTOS] = {.type = PCC_SIG_TYPE_RTOS,
                                  .coalescing_op = PCC_SIG_COALESCING_OP_ACCUM,
                                  .reset_type = PCC_SIG_RESET_TYPE_USR,
                                  .dev_perms = PCC_SIG_WRITE,
                                  .user_perms = PCC_SIG_READ | PCC_SIG_WRITE,
                                  .dtype = PCC_SIG_DTYPE_UINT64,
                                  .init_value = 0}};

    struct pcc_algorithm_handler_attrs attrs = {
        .signals_mask = RENO_PCC_SIGNALS,
        .signal_attrs = signal_attrs,
        .num_signals = RENO_NUM_SIGNALS,
        .user_data_init = &initial_reno_flow_values,
        .user_data_size = sizeof(struct reno_flow_ctx),
        .cc_handler_fn = reno_algo_handler};

    if (pcc_algorithm_handler_install(dev_ctx, &attrs, algo_handler)) {
        fprintf(stderr, "Failed to install Reno algorithm handler\n");
        return 1;
    }

    if (pcc_dev_ctx_caps_free(caps)) {
        fprintf(stderr, "Failed to cleanup device caps\n");
        return 1;
    }

    return 0;
}

int reno_algo_destroy(struct pcc_algorithm_handler_obj *algo_handler) {
    if (pcc_algorithm_handler_remove(algo_handler)) {
        fprintf(stderr, "Failed to remove Reno algo handler\n");
        return 1;
    }
    return 0;
}