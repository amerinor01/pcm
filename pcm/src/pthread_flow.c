#include "pthread_flow.h"
#include "util.h"

void pthrd_flow_signal_elapsed_time_accumulation_op(
    flow_t *flow, const struct signal_attr *attr, int signal) {
    (void)signal;
    struct pthrd_flow *flow_ctx = (struct pthrd_flow *)(flow->backend_ctx);
    struct timespec now_ts;
    if (clock_gettime(CLOCK_MONOTONIC, &now_ts)) {
        LOG_FATAL("clock_gettime failed");
    }
    flow_ctx->signals[attr->metadata.index] =
        CLOCK_GETTIME_TS_DIFF_GET(flow_ctx->start_ts, now_ts);
}

bool pthrd_flow_signal_trigger_timer_check(const flow_t *flow,
                                           const struct signal_attr *attr) {
    struct pthrd_flow *flow_ctx = (struct pthrd_flow *)(flow->backend_ctx);
    int timer = flow_ctx->signals[attr->metadata.index];
    int threshold = flow_ctx->thresholds[attr->metadata.index];
    if (timer) {
        struct timespec now_ts;
        if (clock_gettime(CLOCK_MONOTONIC, &now_ts)) {
            LOG_FATAL("clock_gettime failed");
        }
        if (CLOCK_GETTIME_TS_DIFF_GET(flow_ctx->start_ts, now_ts) - timer >=
            threshold) {
            LOG_DBG("TIMER EXPIRED: now=%d timer=%d threshold=%d",
                    (int)CLOCK_GETTIME_TS_DIFF_GET(flow_ctx->start_ts, now_ts),
                    timer, threshold);
            return true;
        }
    }
    return false;
}

void pthrd_flow_signal_trigger_timer_reset(flow_t *flow,
                                           const struct signal_attr *attr) {
    struct pthrd_flow *flow_ctx = (struct pthrd_flow *)(flow->backend_ctx);
    int timer = flow_ctx->signals[attr->metadata.index];
    if (timer) {
        struct timespec now_ts;
        if (clock_gettime(CLOCK_MONOTONIC, &now_ts)) {
            LOG_FATAL("clock_gettime failed");
        }
        flow_ctx->signals[attr->metadata.index] =
            CLOCK_GETTIME_TS_DIFF_GET(flow_ctx->start_ts, now_ts);
    }
}

bool pthrd_flow_is_ready(const flow_t *flow) {
    struct pthrd_flow *flow_ctx = (struct pthrd_flow *)(flow->backend_ctx);
    return flow_ctx->running;
}

int pthrd_flow_destroy(flow_t *flow) {
    int ret = SUCCESS;
    struct pthrd_flow *flow_ctx = (struct pthrd_flow *)(flow->backend_ctx);

    flow_ctx->running = false;
    if (pthread_join(flow_ctx->pthread_obj, NULL)) {
        LOG_CRIT("[flow=%p addr=%u] flow thread join failed", flow, flow->addr);
        ret = ERROR;
    }

    free(flow->backend_ctx);

    return ret;
}

int pthrd_flow_create(flow_t *flow, traffic_gen_fn_t traffic_gen_fn) {
    flow->backend_ctx = calloc(1, sizeof(struct pthrd_flow));
    if (!flow->backend_ctx) {
        LOG_CRIT("failed to allocate new pthread flow context");
        return ERROR;
    }

    struct pthrd_flow *flow_ctx = (struct pthrd_flow *)flow->backend_ctx;

    ATTR_LIST_FLOW_STATE_INIT(&flow->config->signals_list, struct signal_attr,
                              flow_ctx->thresholds);
    ATTR_LIST_FLOW_STATE_INIT(&flow->config->controls_list, struct control_attr,
                              flow_ctx->controls);
    ATTR_LIST_FLOW_STATE_INIT(&flow->config->local_state_list,
                              struct local_state_attr, flow_ctx->local_state);
    LOG_DBG("[conf=%p] instantiated config on flow=%p addr=%d", flow->config,
            flow, flow->addr);

    if (pthread_create(&flow_ctx->pthread_obj, NULL, traffic_gen_fn,
                       (void *)flow)) {
        LOG_CRIT("failed to start thread for flow=%p addr=%u", flow,
                 flow->addr);
        return ERROR;
    }

    LOG_DBG("started thread for flow=%p addr=%u", flow, flow->addr);

    // Initialize time related signals before traffic generation starts
    if (clock_gettime(CLOCK_MONOTONIC, &flow_ctx->start_ts))
        goto thread_destroy;

    // Arm all triggers, so that all timers/counters could start
    flow_triggers_arm(flow);

    // Mark flow as ready to generate traffic
    flow_ctx->running = true;

    return SUCCESS;

thread_destroy:
    return pthrd_flow_destroy(flow);
}

PLUGIN_FLOW_SIGNAL_GET_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_SIGNAL_SET_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_SIGNAL_UPDATE_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_CONTROL_GET_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_CONTROL_SET_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_LOCAL_STATE_INT_GET_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_LOCAL_STATE_INT_SET_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_LOCAL_STATE_FLOAT_GET_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_LOCAL_STATE_FLOAT_SET_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_ACCUMULATION_OP_SUM_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_ACCUMULATION_OP_LAST_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_ACCUMULATION_OP_MIN_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_ACCUMULATION_OP_MAX_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_TRIGGER_OVERFLOW_CHECK_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_TRIGGER_BURST_RESET_GENERIC_DEFINE(pthrd)
PLUGIN_FLOW_TRIGGER_BURST_CHECK_GENERIC_DEFINE(pthrd)

const char *pthrd_flow_plugin_name = "pthread";

struct flow_plugin_ops pthrd_flow_ops = {
    .control.create = pthrd_flow_create,
    .control.destroy = pthrd_flow_destroy,
    .control.is_ready = pthrd_flow_is_ready,

    .datapath.burst_check = PLUGIN_FLOW_TRIGGER_BURST_CHECK_GENERIC_FN(pthrd),
    .datapath.burst_reset = PLUGIN_FLOW_TRIGGER_BURST_RESET_GENERIC_FN(pthrd),
    .datapath.last = PLUGIN_FLOW_ACCUMULATION_OP_LAST_GENERIC_FN(pthrd),
    .datapath.max = PLUGIN_FLOW_ACCUMULATION_OP_MAX_GENERIC_FN(pthrd),
    .datapath.min = PLUGIN_FLOW_ACCUMULATION_OP_MIN_GENERIC_FN(pthrd),
    .datapath.sum = PLUGIN_FLOW_ACCUMULATION_OP_SUM_GENERIC_FN(pthrd),
    .datapath.overflow_check =
        PLUGIN_FLOW_TRIGGER_OVERFLOW_CHECK_GENERIC_FN(pthrd),
    .datapath.elapsed_time = pthrd_flow_signal_elapsed_time_accumulation_op,
    .datapath.timer_check = pthrd_flow_signal_trigger_timer_check,
    .datapath.timer_reset = pthrd_flow_signal_trigger_timer_reset,

    .handler.control_set = PLUGIN_FLOW_CONTROL_SET_GENERIC_FN(pthrd),
    .handler.control_get = PLUGIN_FLOW_CONTROL_GET_GENERIC_FN(pthrd),
    .handler.signal_set = PLUGIN_FLOW_SIGNAL_SET_GENERIC_FN(pthrd),
    .handler.signal_get = PLUGIN_FLOW_SIGNAL_GET_GENERIC_FN(pthrd),
    .handler.signal_update = PLUGIN_FLOW_SIGNAL_UPDATE_GENERIC_FN(pthrd),
    .handler.local_state_int_get =
        PLUGIN_FLOW_LOCAL_STATE_INT_GET_GENERIC_FN(pthrd),
    .handler.local_state_int_set =
        PLUGIN_FLOW_LOCAL_STATE_INT_SET_GENERIC_FN(pthrd),
    .handler.local_state_float_get =
        PLUGIN_FLOW_LOCAL_STATE_FLOAT_GET_GENERIC_FN(pthrd),
    .handler.local_state_float_set =
        PLUGIN_FLOW_LOCAL_STATE_FLOAT_SET_GENERIC_FN(pthrd)};

int pthrd_flow_ops_init(struct flow_plugin_ops *flow_ops) {
    *flow_ops = pthrd_flow_ops;
    return SUCCESS;
}