#include "htsim_flow.h"
#include "util.h"
#ifdef BUILD_HTSIM_PLUGIN
#include "htsim_pcm_time_wrapper_c.h"
#endif

const char *htsim_flow_plugin_name = "htsim";

#ifndef BUILD_HTSIM_PLUGIN

int htsim_flow_ops_init(struct flow_plugin_ops *flow_ops) {
    (void)flow_ops;
    PCM_LOG_CRIT("PCM library was build without htsim support");
    return PCM_ERROR;
}

#else

int htsim_flow_destroy(pcm_flow_t flow) {
    free(flow->backend_ctx);
    return PCM_SUCCESS;
}

int htsim_flow_create(pcm_flow_t flow, traffic_gen_fn_t traffic_gen_fn) {
    (void)traffic_gen_fn;
    flow->backend_ctx = calloc(1, sizeof(struct htsim_flow));
    if (!flow->backend_ctx) {
        PCM_LOG_CRIT("failed to allocate new pthread flow context");
        return PCM_ERROR;
    }

    struct htsim_flow *flow_ctx = (struct htsim_flow *)flow->backend_ctx;

    ATTR_LIST_FLOW_STATE_INIT(&flow->config->signals_list, struct signal_attr, flow_ctx->thresholds, true);
    ATTR_LIST_FLOW_STATE_INIT(&flow->config->controls_list, struct control_attr, flow_ctx->controls, false);

    PCM_LOG_DBG("[conf=%p] instantiated config on flow=%p addr=%d", flow->config, flow, flow->addr);

    // Initialize time related signals before traffic generation starts
    flow_ctx->start_ts = htsim_now();

    // Arm all triggers, so that all timers/counters could start
    flow_triggers_arm(flow);

    return PCM_SUCCESS;
}

void htsim_snapshot_prepare(pcm_flow_t flow) {
    struct htsim_flow *htsim_ctx = (struct htsim_flow *)flow->backend_ctx;
    memcpy(flow->datapath_snapshot.signals, htsim_ctx->signals, sizeof(uint64_t) * flow->config->num_signals);
    memset(htsim_ctx->signals, 0, sizeof(uint64_t) * flow->config->num_signals);
    memcpy(flow->datapath_snapshot.thresholds, htsim_ctx->thresholds, sizeof(uint64_t) * flow->config->num_signals);
    memcpy(flow->datapath_snapshot.controls, htsim_ctx->controls, sizeof(uint64_t) * flow->config->num_controls);
}

void htsim_snapshot_apply(pcm_flow_t flow) {
    struct htsim_flow *htsim_ctx = (struct htsim_flow *)flow->backend_ctx;
    for (size_t i = 0; i < flow->config->num_signals; i++)
        htsim_ctx->signals[i] += flow->datapath_snapshot.signals[i];
    memcpy(htsim_ctx->thresholds, flow->datapath_snapshot.thresholds, sizeof(uint64_t) * flow->config->num_signals);
    memcpy(htsim_ctx->controls, flow->datapath_snapshot.controls, sizeof(uint64_t) * flow->config->num_controls);
}

PLUGIN_FLOW_CONTROL_SET_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_CONTROL_GET_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_ACCUMULATION_OP_SUM_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_ACCUMULATION_OP_LAST_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_ACCUMULATION_OP_MIN_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_ACCUMULATION_OP_MAX_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_TRIGGER_OVERFLOW_CHECK_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_TRIGGER_BURST_RESET_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_TRIGGER_BURST_CHECK_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_SIGNAL_ELAPSED_TIME_ACCUMULATION_OP_GENERIC_DEFINE(htsim, htsim_now, picosec_ts_diff_us_get)
PLUGIN_FLOW_TRIGGER_TIMER_CHECK_GENERIC_DEFINE(htsim, htsim_now, picosec_ts_diff_us_get)
PLUGIN_FLOW_TRIGGER_TIMER_RESET_GENERIC_DEFINE(htsim, htsim_now, picosec_ts_diff_us_get)
PLUGIN_FLOW_TIME_GET_GENERIC_DEFINE(htsim, htsim_now, picosec_ts_diff_us_get)

struct flow_plugin_ops htsim_flow_ops = {
    .control.create = htsim_flow_create,
    .control.destroy = htsim_flow_destroy,
    .control.is_ready = NULL,
    .control.time_get = PLUGIN_FLOW_TIME_GET_GENERIC_FN(htsim),

    .datapath.burst_check = PLUGIN_FLOW_TRIGGER_BURST_CHECK_GENERIC_FN(htsim),
    .datapath.burst_reset = PLUGIN_FLOW_TRIGGER_BURST_RESET_GENERIC_FN(htsim),
    .datapath.last = PLUGIN_FLOW_ACCUMULATION_OP_LAST_GENERIC_FN(htsim),
    .datapath.max = PLUGIN_FLOW_ACCUMULATION_OP_MAX_GENERIC_FN(htsim),
    .datapath.min = PLUGIN_FLOW_ACCUMULATION_OP_MIN_GENERIC_FN(htsim),
    .datapath.sum = PLUGIN_FLOW_ACCUMULATION_OP_SUM_GENERIC_FN(htsim),
    .datapath.overflow_check = PLUGIN_FLOW_TRIGGER_OVERFLOW_CHECK_GENERIC_FN(htsim),
    .datapath.elapsed_time = PLUGIN_FLOW_SIGNAL_ELAPSED_TIME_ACCUMULATION_OP_GENERIC_FN(htsim),
    .datapath.timer_check = PLUGIN_FLOW_TRIGGER_TIMER_CHECK_GENERIC_FN(htsim),
    .datapath.timer_reset = PLUGIN_FLOW_TRIGGER_TIMER_RESET_GENERIC_FN(htsim),
    .datapath.control_set = PLUGIN_FLOW_CONTROL_SET_GENERIC_FN(htsim),
    .datapath.control_get = PLUGIN_FLOW_CONTROL_GET_GENERIC_FN(htsim),
    .datapath.snapshot_prepare = htsim_snapshot_prepare,
    .datapath.snapshot_apply = htsim_snapshot_apply,
};

int htsim_flow_ops_init(struct flow_plugin_ops *flow_ops) {
    *flow_ops = htsim_flow_ops;
    return PCM_SUCCESS;
}

#endif

__attribute__((constructor)) void htsim_plugin_register(void) {
    flow_plugin_register(htsim_flow_plugin_name, htsim_flow_ops_init);
}

__attribute__((destructor)) void htsim_plugin_deregister(void) { flow_plugin_deregister(htsim_flow_plugin_name); }