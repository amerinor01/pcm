#include "pthread_flow.h"
#include "util.h"

int pthrd_flow_signal_get(const void *ctx, size_t user_index) {
    struct pthrd_flow *flow_ctx =
        ((struct pthrd_flow *)(((struct flow *)ctx)->backend_ctx));
    return flow_ctx->signals[user_index];
}

void pthrd_flow_signal_set(void *ctx, size_t user_index, int val) {
    struct pthrd_flow *flow_ctx =
        ((struct pthrd_flow *)(((struct flow *)ctx)->backend_ctx));
    flow_ctx->signals[user_index] = val;
}

void pthrd_flow_signal_update(void *ctx, size_t user_index, int val) {
    struct pthrd_flow *flow_ctx =
        ((struct pthrd_flow *)(((struct flow *)ctx)->backend_ctx));
    flow_ctx->signals[user_index] += val;
}

int pthrd_flow_control_get(const void *ctx, size_t user_index) {
    struct pthrd_flow *flow_ctx =
        ((struct pthrd_flow *)(((struct flow *)ctx)->backend_ctx));
    return flow_ctx->controls[user_index];
}

void pthrd_flow_control_set(void *ctx, size_t user_index, int val) {
    struct pthrd_flow *flow_ctx =
        ((struct pthrd_flow *)(((struct flow *)ctx)->backend_ctx));
    flow_ctx->controls[user_index] = val;
}

int pthrd_flow_local_state_int_get(const void *ctx, size_t user_index) {
    struct pthrd_flow *flow_ctx =
        ((struct pthrd_flow *)(((struct flow *)ctx)->backend_ctx));
    return decode_int(flow_ctx->local_state[user_index]);
}

void pthrd_flow_local_state_int_set(void *ctx, size_t user_index, int val) {
    struct pthrd_flow *flow_ctx =
        ((struct pthrd_flow *)(((struct flow *)ctx)->backend_ctx));
    flow_ctx->local_state[user_index] = encode_int(val);
}

float pthrd_flow_local_state_float_get(const void *ctx, size_t user_index) {
    struct pthrd_flow *flow_ctx =
        ((struct pthrd_flow *)(((struct flow *)ctx)->backend_ctx));
    return decode_float(flow_ctx->local_state[user_index]);
}

void pthrd_flow_local_state_float_set(void *ctx, size_t user_index, float val) {
    struct pthrd_flow *flow_ctx =
        ((struct pthrd_flow *)(((struct flow *)ctx)->backend_ctx));
    flow_ctx->local_state[user_index] = encode_float(val);
}

void pthrd_flow_signal_accumulation_op_sum(flow_t *flow,
                                           const struct signal_attr *attr,
                                           int signal) {
    struct pthrd_flow *flow_ctx = (struct pthrd_flow *)(flow->backend_ctx);
    flow_ctx->signals[attr->metadata.index] += signal;
}

void pthrd_flow_signal_accumulation_op_last(flow_t *flow,
                                            const struct signal_attr *attr,
                                            int signal) {
    struct pthrd_flow *flow_ctx = (struct pthrd_flow *)(flow->backend_ctx);
    flow_ctx->signals[attr->metadata.index] = signal;
}

void pthrd_flow_signal_accumulation_op_min(flow_t *flow,
                                           const struct signal_attr *attr,
                                           int signal) {
    struct pthrd_flow *flow_ctx = (struct pthrd_flow *)(flow->backend_ctx);
    if (signal < flow_ctx->signals[attr->metadata.index]) {
        flow_ctx->signals[attr->metadata.index] = signal;
    }
}

void pthrd_flow_signal_accumulation_op_max(flow_t *flow,
                                           const struct signal_attr *attr,
                                           int signal) {
    struct pthrd_flow *flow_ctx = (struct pthrd_flow *)(flow->backend_ctx);
    if (signal > flow_ctx->signals[attr->metadata.index]) {
        flow_ctx->signals[attr->metadata.index] = signal;
    }
}

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

bool pthrd_flow_signal_trigger_overflow_check(const flow_t *flow,
                                              const struct signal_attr *attr) {
    struct pthrd_flow *flow_ctx = (struct pthrd_flow *)(flow->backend_ctx);
    int value = flow_ctx->signals[attr->metadata.index];
    int threshold = flow_ctx->thresholds[attr->metadata.index];
    if (value >= threshold)
        return true;
    return false;
}

void pthrd_flow_signal_trigger_burst_reset(flow_t *flow,
                                           const struct signal_attr *attr) {
    struct pthrd_flow *flow_ctx = (struct pthrd_flow *)(flow->backend_ctx);
    int burst = flow_ctx->signals[attr->metadata.index];
    if (burst) {
        // post reset state of 1 indicates that burst acts as an active trigger
        flow_ctx->signals[attr->metadata.index] = 1;
    }
}

bool pthrd_flow_signal_trigger_burst_check(const flow_t *flow,
                                           const struct signal_attr *attr) {
    struct pthrd_flow *flow_ctx = (struct pthrd_flow *)(flow->backend_ctx);
    int burst = flow_ctx->signals[attr->metadata.index];
    int threshold = flow_ctx->thresholds[attr->metadata.index];
    if (burst) {
        // subtract 1 here to remove the original activation flag
        if ((burst - 1) >= threshold) {
            return true;
        }
    }
    return false;
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
            LOG_INFO("TIMER EXPIRED: now=%d timer=%d threshold=%d",
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

const char *pthrd_flow_plugin_name = "pthread";

struct flow_plugin_ops pthrd_flow_ops = {
    .control.create = pthrd_flow_create,
    .control.destroy = pthrd_flow_destroy,
    .control.is_ready = pthrd_flow_is_ready,

    .datapath.burst_check = pthrd_flow_signal_trigger_burst_check,
    .datapath.burst_reset = pthrd_flow_signal_trigger_burst_reset,
    .datapath.elapsed_time = pthrd_flow_signal_elapsed_time_accumulation_op,
    .datapath.last = pthrd_flow_signal_accumulation_op_last,
    .datapath.max = pthrd_flow_signal_accumulation_op_max,
    .datapath.min = pthrd_flow_signal_accumulation_op_min,
    .datapath.sum = pthrd_flow_signal_accumulation_op_sum,
    .datapath.overflow_check = pthrd_flow_signal_trigger_overflow_check,
    .datapath.timer_check = pthrd_flow_signal_trigger_timer_check,
    .datapath.timer_reset = pthrd_flow_signal_trigger_timer_reset,

    .handler.control_set = pthrd_flow_control_set,
    .handler.control_get = pthrd_flow_control_get,
    .handler.signal_set = pthrd_flow_signal_set,
    .handler.signal_get = pthrd_flow_signal_get,
    .handler.signal_update = pthrd_flow_signal_update,
    .handler.local_state_int_get = pthrd_flow_local_state_int_get,
    .handler.local_state_int_set = pthrd_flow_local_state_int_set,
    .handler.local_state_float_get = pthrd_flow_local_state_float_get,
    .handler.local_state_float_set = pthrd_flow_local_state_float_set};

int pthrd_flow_ops_init(struct flow_plugin_ops *flow_ops) {
    *flow_ops = pthrd_flow_ops;
    return SUCCESS;
}