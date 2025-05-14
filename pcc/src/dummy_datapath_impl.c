#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "dummy_datapath_impl.h"
#include "pcc_sw_ctx.h"

static int global_flow_id = 0;

static inline uint64_t _flow_signal_read_u64(struct pcc_flow_ctx *ctx,
                                             size_t idx) {
    return atomic_load(&((struct dummy_signal_storage *)ctx->signal_storage)
                            ->signals_u64[idx]);
}

uint64_t pcc_flow_signal_read_u64(struct pcc_flow_ctx *ctx, size_t idx) {
    return _flow_signal_read_u64(ctx, idx);
}

void pcc_flow_signal_write_u64(struct pcc_flow_ctx *ctx, size_t idx,
                               uint64_t val) {
    atomic_store(
        &((struct dummy_signal_storage *)ctx->signal_storage)->signals_u64[idx],
        val);
}

void *pcc_flow_user_data_ptr_get(struct pcc_flow_ctx *ctx) {
    return ((struct dummy_signal_storage *)ctx->signal_storage)->user_data;
}

/*
static void *dummy_flow_thread_fn(void *arg) {
    struct dummy_flow *flow = arg;
    struct pcc_flow_ctx *pcc_ctx = flow->pcc_ctx;
    struct dummy_signal_storage *signal_storage =
        (struct dummy_signal_storage *)flow->pcc_ctx->signal_storage;
    printf("[flow=%d] start\n", flow->fid);
    while (atomic_load(&flow->running)) {
        atomic_fetch_add(&signal_storage->signals_u64[signal_storage->acks_idx],
                         1);
        uint64_t cwnd =
            _flow_signal_read_u64(pcc_ctx, signal_storage->cwnd_idx);
        fprintf(stderr, "[flow=%d] cwnd: %llu\n", flow->fid, cwnd);
        usleep(1000); // 1ms
    }
    printf("[flow=%d] termination\n", flow->fid);
    return NULL;
}
*/

// Flow thread: emulate bandwidth, drops, NACKs, RTOs
static void *dummy_flow_thread_fn(void *arg) {
    struct dummy_flow *flow = arg;
    struct pcc_flow_ctx *pcc_ctx = flow->pcc_ctx;
    struct dummy_signal_storage *signal_storage =
        (struct dummy_signal_storage *)flow->pcc_ctx->signal_storage;
    const uint64_t pkts_per_ms =
        (DEFAULT_BANDWIDTH_BPS / 8 / 1000) / DEFAULT_PACKET_SIZE;
    unsigned int rnd = (unsigned int)time(NULL) ^ (unsigned int)(uintptr_t)flow;
    printf("[flow=%d] start\n", flow->fid);
    while (atomic_load(&flow->running)) {
        uint64_t cwnd =
            _flow_signal_read_u64(pcc_ctx, signal_storage->cwnd_idx);
        if (cwnd == 0) {
            usleep(1000);
            continue;
        }
        uint64_t to_send = cwnd < pkts_per_ms ? cwnd : pkts_per_ms;
        double roll = (double)rand_r(&rnd) / RAND_MAX;
        if (roll < DEFAULT_DROP_PROB) {
            atomic_fetch_add(
                &signal_storage->signals_u64[signal_storage->rto_idx], 1);
        } else if (roll < DEFAULT_DROP_PROB + DEFAULT_NACK_PROB) {
            atomic_fetch_add(
                &signal_storage->signals_u64[signal_storage->nacks_idx], 1);
        } else {
            atomic_fetch_add(
                &signal_storage->signals_u64[signal_storage->acks_idx],
                to_send);
        }
        usleep(1000); // 1 ms
        fprintf(stderr, "[flow=%d] pkts_per_ms=%llu, to_send=%llu, cwnd=%llu\n",
                flow->fid, pkts_per_ms, to_send, cwnd);
    }
    printf("[flow=%d] termination\n", flow->fid);
    return NULL;
}

static ssize_t signal_idx_find(struct dummy_signal_storage *ctx,
                               uint32_t sig_type) {
    for (size_t i = 0; i < ctx->num_signals; i++) {
        if (ctx->handler->attrs.signal_attrs[i].type == sig_type) {
            return i;
        }
    }
    return -1;
}

int dummy_flow_create(struct pcc_dev_ctx *dev, struct dummy_flow **flow) {
    if (!dev->handler)
        return PCC_ERROR;

    struct dummy_flow *new_flow = calloc(1, sizeof(*new_flow));
    if (!new_flow)
        return PCC_ERROR;

    // Init signals
    struct dummy_signal_storage *signal_storage =
        calloc(1, sizeof(*signal_storage));
    if (!signal_storage)
        goto flow_cleanup;
    signal_storage->handler = dev->handler;
    signal_storage->num_signals = dev->handler->attrs.num_signals;
    signal_storage->signals_u64 = calloc(signal_storage->num_signals,
                                         sizeof(*signal_storage->signals_u64));
    if (!signal_storage->signals_u64)
        goto signal_storage_cleanup;
    for (size_t i = 0; i < signal_storage->num_signals; i++) {
        uint64_t init = dev->handler->attrs.signal_attrs[i].init_value;
        atomic_init(&signal_storage->signals_u64[i], init);
    }
    signal_storage->cwnd_idx =
        signal_idx_find(signal_storage, PCC_SIG_TYPE_CWND);
    signal_storage->acks_idx =
        signal_idx_find(signal_storage, PCC_SIG_TYPE_ACKS_RECVD);
    signal_storage->nacks_idx =
        signal_idx_find(signal_storage, PCC_SIG_TYPE_NACKS_RECVD);
    signal_storage->rto_idx =
        signal_idx_find(signal_storage, PCC_SIG_TYPE_RTOS);
    if (signal_storage->cwnd_idx == -1 || signal_storage->acks_idx == -1 ||
        signal_storage->nacks_idx == -1 || signal_storage->rto_idx == -1)
        goto signal_storage_cleanup;

    // Init algorithm per-flow data
    signal_storage->user_data = calloc(1, dev->handler->attrs.user_data_size);
    if (!signal_storage->user_data)
        goto signals_cleanup;
    if (dev->handler->attrs.user_data_init) {
        memcpy(signal_storage->user_data, dev->handler->attrs.user_data_init,
               dev->handler->attrs.user_data_size);
    }

    // Allocate generic per-flow data handler
    struct pcc_flow_ctx *pcc_ctx = calloc(1, sizeof(*pcc_ctx));
    if (!pcc_ctx)
        goto usr_data_cleanup;
    pcc_ctx->signal_storage = (void *)signal_storage;
    new_flow->pcc_ctx = pcc_ctx;
    new_flow->dev = dev;
    new_flow->fid = global_flow_id++;

    // Add flow to the scheduler
    if (pcc_scheduler_flow_add(new_flow->dev, new_flow->pcc_ctx))
        goto usr_data_cleanup;

    // Now flow can generate new events, e.g., traffic
    atomic_store(&new_flow->running, true);
    if (pthread_create(&new_flow->thread, NULL, dummy_flow_thread_fn,
                       (void *)new_flow))
        goto pcc_sched_cleanup;

    *flow = new_flow;

    return PCC_SUCCESS;

pcc_sched_cleanup:
    pcc_scheduler_flow_remove(new_flow->dev, new_flow->pcc_ctx);
pcc_ctx_cleanup:
    free(new_flow->pcc_ctx);
usr_data_cleanup:
    free(signal_storage->user_data);
signals_cleanup:
    free(signal_storage->signals_u64);
signal_storage_cleanup:
    free(signal_storage);
flow_cleanup:
    free(new_flow);

    return PCC_ERROR;
}

int dummy_flow_destroy(struct dummy_flow *flow) {
    int ret = PCC_SUCCESS;

    if (pcc_scheduler_flow_remove(flow->dev, flow->pcc_ctx)) {
        fprintf(stderr, "failed to deschedule flow\n");
        ret = PCC_ERROR;
    }

    atomic_store(&flow->running, false);
    if (pthread_join(flow->thread, NULL)) {
        fprintf(stderr, "flow thread join failed\n");
        ret = PCC_ERROR;
    }

    struct dummy_signal_storage *signal_storage = flow->pcc_ctx->signal_storage;
    free(signal_storage->user_data);
    free(signal_storage->signals_u64);
    free(signal_storage);
    free(flow->pcc_ctx);
    free(flow);

    return ret;
}