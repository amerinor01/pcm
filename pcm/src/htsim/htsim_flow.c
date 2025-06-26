#include "htsim_flow.h"
#include "../util.h"
#ifdef BUILD_HTSIM_FLOW_PLUGIN
#include "htsim_pcm_time_wrapper_c.h"
#endif

const char *htsim_flow_plugin_name = "htsim";

#ifndef BUILD_HTSIM_FLOW_PLUGIN

int htsim_flow_ops_init(struct flow_plugin_ops *flow_ops) {
    (void)flow_ops;
    LOG_CRIT("PCM library was build without htsim support");
    return ERROR;
}

#else

int htsim_flow_destroy(flow_t *flow) {
    free(flow->backend_ctx);
    return SUCCESS;
}

int htsim_flow_create(flow_t *flow, traffic_gen_fn_t traffic_gen_fn) {
    (void)traffic_gen_fn;
    flow->backend_ctx = calloc(1, sizeof(struct htsim_flow));
    if (!flow->backend_ctx) {
        LOG_CRIT("failed to allocate new pthread flow context");
        return ERROR;
    }

    struct htsim_flow *flow_ctx = (struct htsim_flow *)flow->backend_ctx;

    ATTR_LIST_FLOW_STATE_INIT(&flow->config->signals_list, struct signal_attr,
                              flow_ctx->thresholds);
    ATTR_LIST_FLOW_STATE_INIT(&flow->config->controls_list, struct control_attr,
                              flow_ctx->controls);
    ATTR_LIST_FLOW_STATE_INIT(&flow->config->local_state_list,
                              struct local_state_attr, flow_ctx->local_state);
    LOG_DBG("[conf=%p] instantiated config on flow=%p addr=%d", flow->config,
            flow, flow->addr);

    // Initialize time related signals before traffic generation starts
    flow_ctx->start_ts = htsim_now();

    // Arm all triggers, so that all timers/counters could start
    flow_triggers_arm(flow);

    return SUCCESS;
}

PLUGIN_FLOW_SIGNAL_GET_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_SIGNAL_SET_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_SIGNAL_UPDATE_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_CONTROL_GET_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_CONTROL_SET_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_LOCAL_STATE_INT_GET_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_LOCAL_STATE_INT_SET_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_LOCAL_STATE_FLOAT_GET_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_LOCAL_STATE_FLOAT_SET_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_ACCUMULATION_OP_SUM_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_ACCUMULATION_OP_LAST_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_ACCUMULATION_OP_MIN_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_ACCUMULATION_OP_MAX_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_TRIGGER_OVERFLOW_CHECK_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_TRIGGER_BURST_RESET_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_TRIGGER_BURST_CHECK_GENERIC_DEFINE(htsim)
PLUGIN_FLOW_SIGNAL_ELAPSED_TIME_ACCUMULATION_OP_GENERIC_DEFINE(
    htsim, htsim_now, picosec_ts_diff_us_get)
PLUGIN_FLOW_TRIGGER_TIMER_CHECK_GENERIC_DEFINE(htsim, htsim_now,
                                               picosec_ts_diff_us_get)
PLUGIN_FLOW_TRIGGER_TIMER_RESET_GENERIC_DEFINE(htsim, htsim_now,
                                               picosec_ts_diff_us_get)

struct flow_plugin_ops htsim_flow_ops = {
    .control.create = htsim_flow_create,
    .control.destroy = htsim_flow_destroy,
    .control.is_ready = NULL,

    .datapath.burst_check = PLUGIN_FLOW_TRIGGER_BURST_CHECK_GENERIC_FN(htsim),
    .datapath.burst_reset = PLUGIN_FLOW_TRIGGER_BURST_RESET_GENERIC_FN(htsim),
    .datapath.last = PLUGIN_FLOW_ACCUMULATION_OP_LAST_GENERIC_FN(htsim),
    .datapath.max = PLUGIN_FLOW_ACCUMULATION_OP_MAX_GENERIC_FN(htsim),
    .datapath.min = PLUGIN_FLOW_ACCUMULATION_OP_MIN_GENERIC_FN(htsim),
    .datapath.sum = PLUGIN_FLOW_ACCUMULATION_OP_SUM_GENERIC_FN(htsim),
    .datapath.overflow_check =
        PLUGIN_FLOW_TRIGGER_OVERFLOW_CHECK_GENERIC_FN(htsim),
    .datapath.elapsed_time =
        PLUGIN_FLOW_SIGNAL_ELAPSED_TIME_ACCUMULATION_OP_GENERIC_FN(htsim),
    .datapath.timer_check = PLUGIN_FLOW_TRIGGER_TIMER_CHECK_GENERIC_FN(htsim),
    .datapath.timer_reset = PLUGIN_FLOW_TRIGGER_TIMER_RESET_GENERIC_FN(htsim),

    .handler.control_set = PLUGIN_FLOW_CONTROL_SET_GENERIC_FN(htsim),
    .handler.control_get = PLUGIN_FLOW_CONTROL_GET_GENERIC_FN(htsim),
    .handler.signal_set = PLUGIN_FLOW_SIGNAL_SET_GENERIC_FN(htsim),
    .handler.signal_get = PLUGIN_FLOW_SIGNAL_GET_GENERIC_FN(htsim),
    .handler.signal_update = PLUGIN_FLOW_SIGNAL_UPDATE_GENERIC_FN(htsim),
    .handler.local_state_int_get =
        PLUGIN_FLOW_LOCAL_STATE_INT_GET_GENERIC_FN(htsim),
    .handler.local_state_int_set =
        PLUGIN_FLOW_LOCAL_STATE_INT_SET_GENERIC_FN(htsim),
    .handler.local_state_float_get =
        PLUGIN_FLOW_LOCAL_STATE_FLOAT_GET_GENERIC_FN(htsim),
    .handler.local_state_float_set =
        PLUGIN_FLOW_LOCAL_STATE_FLOAT_SET_GENERIC_FN(htsim)};

int htsim_flow_ops_init(struct flow_plugin_ops *flow_ops) {
    *flow_ops = htsim_flow_ops;
    return SUCCESS;
}

#endif