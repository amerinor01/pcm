#include "pthread_flow.h"
#include "util.h"

const char *pthrd_flow_plugin_name = "pthread";

bool pthrd_flow_is_ready(const pcm_flow_t flow) {
    struct pthrd_flow *flow_ctx = (struct pthrd_flow *)(flow->backend_ctx);
    return flow_ctx->running;
}

int pthrd_flow_destroy(pcm_flow_t flow) {
    int ret = PCM_SUCCESS;
    struct pthrd_flow *flow_ctx = (struct pthrd_flow *)(flow->backend_ctx);

    flow_ctx->running = false;
    if (pthread_join(flow_ctx->pthread_obj, NULL)) {
        PCM_LOG_CRIT("[flow=%p addr=%u] flow thread join failed", flow,
                     flow->addr);
        ret = PCM_ERROR;
    }

    free(flow->backend_ctx);

    return ret;
}

int pthrd_flow_create(pcm_flow_t flow, traffic_gen_fn_t traffic_gen_fn) {
    flow->backend_ctx = calloc(1, sizeof(struct pthrd_flow));
    if (!flow->backend_ctx) {
        PCM_LOG_CRIT("failed to allocate new pthread flow context");
        return PCM_ERROR;
    }

    struct pthrd_flow *flow_ctx = (struct pthrd_flow *)flow->backend_ctx;

    ATTR_LIST_FLOW_STATE_INIT(&flow->config->signals_list, struct signal_attr,
                              flow_ctx->thresholds, true);
    ATTR_LIST_FLOW_STATE_INIT(&flow->config->controls_list, struct control_attr,
                              flow_ctx->controls, false);

    PCM_LOG_DBG("[conf=%p] instantiated config on flow=%p addr=%d",
                flow->config, flow, flow->addr);

    if (pthread_create(&flow_ctx->pthread_obj, NULL, traffic_gen_fn,
                       (void *)flow)) {
        PCM_LOG_CRIT("failed to start thread for flow=%p addr=%u", flow,
                     flow->addr);
        return PCM_ERROR;
    }

    PCM_LOG_DBG("started thread for flow=%p addr=%u", flow, flow->addr);

    // Initialize time related signals before traffic generation starts
    if (clock_gettime(CLOCK_MONOTONIC, &flow_ctx->start_ts))
        goto thread_destroy;

    // Arm all triggers, so that all timers/counters could start
    flow_triggers_arm(flow);

    // Mark flow as ready to generate traffic
    flow_ctx->running = true;

    return PCM_SUCCESS;

thread_destroy:
    return pthrd_flow_destroy(flow);
}

void pthrd_snapshot_prepare(pcm_flow_t flow) {
    struct pthrd_flow *flow_ctx = (struct pthrd_flow *)flow->backend_ctx;
    for (size_t i = 0; i < flow->config->num_signals; i++) {
        flow->datapath_snapshot.signals[i] =
            atomic_exchange(&flow_ctx->signals[i], 0);
        flow->datapath_snapshot.thresholds[i] = flow_ctx->thresholds[i];
    }
    for (size_t i = 0; i < flow->config->num_controls; i++) {
        flow->datapath_snapshot.controls[i] = flow_ctx->controls[i];
    }
}

void pthrd_snapshot_apply(pcm_flow_t flow) {
    struct pthrd_flow *flow_ctx = (struct pthrd_flow *)flow->backend_ctx;
    for (size_t i = 0; i < flow->config->num_signals; i++) {
        flow_ctx->signals[i] += flow->datapath_snapshot.signals[i];
        flow_ctx->thresholds[i] = flow->datapath_snapshot.thresholds[i];
    }
    for (size_t i = 0; i < flow->config->num_controls; i++) {
        flow_ctx->controls[i] = flow->datapath_snapshot.controls[i];
    }
}

PLUGIN_FLOW_CONTROL_SET_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_CONTROL_GET_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_ACCUMULATION_OP_SUM_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_ACCUMULATION_OP_LAST_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_ACCUMULATION_OP_MIN_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_ACCUMULATION_OP_MAX_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_TRIGGER_OVERFLOW_CHECK_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_TRIGGER_BURST_RESET_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_TRIGGER_BURST_CHECK_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_SIGNAL_ELAPSED_TIME_ACCUMULATION_OP_GENERIC_DEFINE(
    pthrd, clock_gettime_now, clock_gettime_ts_diff_us_get)
PLUGIN_FLOW_TRIGGER_TIMER_CHECK_GENERIC_DEFINE(pthrd, clock_gettime_now,
                                               clock_gettime_ts_diff_us_get)
PLUGIN_FLOW_TRIGGER_TIMER_RESET_GENERIC_DEFINE(pthrd, clock_gettime_now,
                                               clock_gettime_ts_diff_us_get)
PLUGIN_FLOW_TIME_GET_GENERIC_DEFINE(pthrd, clock_gettime_now,
                                    clock_gettime_ts_diff_us_get)

struct flow_plugin_ops pthrd_flow_ops = {
    .control.create = pthrd_flow_create,
    .control.destroy = pthrd_flow_destroy,
    .control.is_ready = pthrd_flow_is_ready,
    .control.time_get = PLUGIN_FLOW_TIME_GET_GENERIC_FN(pthrd),

    .datapath.burst_check = PLUGIN_FLOW_TRIGGER_BURST_CHECK_GENERIC_FN(pthrd),
    .datapath.burst_reset = PLUGIN_FLOW_TRIGGER_BURST_RESET_GENERIC_FN(pthrd),
    .datapath.last = PLUGIN_FLOW_ACCUMULATION_OP_LAST_GENERIC_FN(pthrd),
    .datapath.max = PLUGIN_FLOW_ACCUMULATION_OP_MAX_GENERIC_FN(pthrd),
    .datapath.min = PLUGIN_FLOW_ACCUMULATION_OP_MIN_GENERIC_FN(pthrd),
    .datapath.sum = PLUGIN_FLOW_ACCUMULATION_OP_SUM_GENERIC_FN(pthrd),
    .datapath.overflow_check =
        PLUGIN_FLOW_TRIGGER_OVERFLOW_CHECK_GENERIC_FN(pthrd),
    .datapath.elapsed_time =
        PLUGIN_FLOW_SIGNAL_ELAPSED_TIME_ACCUMULATION_OP_GENERIC_FN(pthrd),
    .datapath.timer_check = PLUGIN_FLOW_TRIGGER_TIMER_CHECK_GENERIC_FN(pthrd),
    .datapath.timer_reset = PLUGIN_FLOW_TRIGGER_TIMER_RESET_GENERIC_FN(pthrd),
    .datapath.control_set = PLUGIN_FLOW_CONTROL_SET_GENERIC_FN(pthrd),
    .datapath.control_get = PLUGIN_FLOW_CONTROL_GET_GENERIC_FN(pthrd),
    .datapath.snapshot_prepare = pthrd_snapshot_prepare,
    .datapath.snapshot_apply = pthrd_snapshot_apply,
};

int pthrd_flow_ops_init(struct flow_plugin_ops *flow_ops) {
    *flow_ops = pthrd_flow_ops;
    return PCM_SUCCESS;
}

__attribute__((constructor)) void pthrd_plugin_register(void) {
    flow_plugin_register(pthrd_flow_plugin_name, pthrd_flow_ops_init);
}

__attribute__((destructor)) void pthrd_plugin_deregister(void) {
    flow_plugin_deregister(pthrd_flow_plugin_name);
}