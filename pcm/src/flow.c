#include <unistd.h>

#include "impl.h"
#include "util.h"

signal_trigger_rearm_fn flow_signal_trigger_rearm_no_op = NULL;
signal_accumulation_op_fn flow_signal_accumulation_no_op = NULL;

void flow_signal_accumulation_op_sum(flow_t *flow,
                                     const struct signal_attr *attr,
                                     int signal) {
    flow->datapath_state[FLOW_SIGNALS_OFFSET + attr->metadata.index] += signal;
}

void flow_signal_accumulation_op_last(flow_t *flow,
                                      const struct signal_attr *attr,
                                      int signal) {
    flow->datapath_state[FLOW_SIGNALS_OFFSET + attr->metadata.index] = signal;
}

void flow_signal_accumulation_op_min(flow_t *flow,
                                     const struct signal_attr *attr,
                                     int signal) {
    if (signal <
        flow->datapath_state[FLOW_SIGNALS_OFFSET + attr->metadata.index])
        flow->datapath_state[FLOW_SIGNALS_OFFSET + attr->metadata.index] =
            signal;
}

void flow_signal_accumulation_op_max(flow_t *flow,
                                     const struct signal_attr *attr,
                                     int signal) {
    if (signal >
        flow->datapath_state[FLOW_SIGNALS_OFFSET + attr->metadata.index])
        flow->datapath_state[FLOW_SIGNALS_OFFSET + attr->metadata.index] =
            signal;
}

void flow_signal_elapsed_time_accumulation_op(flow_t *flow,
                                              const struct signal_attr *attr,
                                              int signal) {
    (void)signal;
    struct timespec now_ts;
    if (clock_gettime(CLOCK_MONOTONIC, &now_ts)) {
        LOG_FATAL("clock_gettime failed");
    }
    flow->datapath_state[FLOW_SIGNALS_OFFSET + attr->metadata.index] =
        CLOCK_GETTIME_TS_DIFF_GET(flow->start_ts, now_ts);
}

bool flow_signal_trigger_overflow_check(const flow_t *flow,
                                        const struct signal_attr *attr) {
    int value =
        flow->datapath_state[FLOW_SIGNALS_OFFSET + attr->metadata.index];
    int threshold = flow->datapath_state[FLOW_SIGNALS_THRESHOLDS_OFFSET +
                                         attr->metadata.index];
    if (value >= threshold)
        return true;
    return false;
}

void flow_signal_trigger_burst_reset(flow_t *flow,
                                     const struct signal_attr *attr) {
    int burst =
        flow->datapath_state[FLOW_SIGNALS_OFFSET + attr->metadata.index];
    if (burst) {
        // post reset state of 1 indicates that burst acts as an active trigger
        flow->datapath_state[FLOW_SIGNALS_OFFSET + attr->metadata.index] = 1;
    }
}

bool flow_signal_trigger_burst_check(const flow_t *flow,
                                     const struct signal_attr *attr) {
    int burst =
        flow->datapath_state[FLOW_SIGNALS_OFFSET + attr->metadata.index];
    int threshold = flow->datapath_state[FLOW_SIGNALS_THRESHOLDS_OFFSET +
                                         attr->metadata.index];
    if (burst) {
        // subtract 1 here to remove the original activation flag
        if ((burst - 1) >= threshold) {
            return true;
        }
    }
    return false;
}

bool flow_signal_trigger_timer_check(const flow_t *flow,
                                     const struct signal_attr *attr) {
    int timer =
        flow->datapath_state[FLOW_SIGNALS_OFFSET + attr->metadata.index];
    int threshold = flow->datapath_state[FLOW_SIGNALS_THRESHOLDS_OFFSET +
                                         attr->metadata.index];
    if (timer) {
        struct timespec now_ts;
        if (clock_gettime(CLOCK_MONOTONIC, &now_ts)) {
            LOG_FATAL("clock_gettime failed");
        }
        if (CLOCK_GETTIME_TS_DIFF_GET(flow->start_ts, now_ts) - timer >=
            threshold) {
            LOG_INFO("TIMER EXPIRED: now=%d timer=%d threshold=%d",
                     (int)CLOCK_GETTIME_TS_DIFF_GET(flow->start_ts, now_ts),
                     timer, threshold);
            return true;
        }
    }
    return false;
}

void flow_signal_trigger_timer_reset(flow_t *flow,
                                     const struct signal_attr *attr) {
    int timer =
        flow->datapath_state[FLOW_SIGNALS_OFFSET + attr->metadata.index];
    if (timer) {
        struct timespec now_ts;
        if (clock_gettime(CLOCK_MONOTONIC, &now_ts)) {
            LOG_FATAL("clock_gettime failed");
        }
        flow->datapath_state[FLOW_SIGNALS_OFFSET + attr->metadata.index] =
            CLOCK_GETTIME_TS_DIFF_GET(flow->start_ts, now_ts);
    }
}

void flow_state_init(const struct algorithm_config *config, flow_t *flow) {
    ATTR_LIST_FLOW_STATE_INIT(&config->signals_list, struct signal_attr,
                              flow->datapath_state,
                              FLOW_SIGNALS_THRESHOLDS_OFFSET);
    ATTR_LIST_FLOW_STATE_INIT(&config->controls_list, struct control_attr,
                              flow->datapath_state, FLOW_CONTROLS_OFFSET);
    ATTR_LIST_FLOW_STATE_INIT(&config->local_state_list,
                              struct local_state_attr, flow->local_state,
                              FLOW_LOCAL_STATE_VARS_OFFSET);
    LOG_DBG("[dev=%p conf=%p] instantiated config on flow=%p addr=%d",
            config->device, config, flow, flow->addr);
}

int flow_create(device_t *device, flow_t **flow,
                traffic_gen_fn_t traffic_gen_fn) {
    if (!device)
        return ERROR;

    flow_t *new_flow = calloc(1, sizeof(*new_flow));
    if (!new_flow) {
        LOG_CRIT("[dev=%p] failed to allocate new flow", device);
        return ERROR;
    }

    new_flow->addr = device->flow_addr_counter++;
    LOG_DBG("[dev=%p] allocated new flow=%p, addr=%u", device, new_flow,
            new_flow->addr);

    new_flow->config = device_flow_id_to_config_match(device, new_flow->addr);
    if (!new_flow->config) {
        LOG_CRIT("[dev=%p] failed to match flow addr=%u to algorithm config",
                 device, new_flow->addr);
        goto free_flow;
    }

    flow_state_init(new_flow->config, new_flow);

    flow_state_set(new_flow, FLOW_STATE_INIT);
    if (pthread_create(&new_flow->thread, NULL, traffic_gen_fn,
                       (void *)new_flow)) {
        LOG_CRIT("[dev=%p] failed to start thread for flow=%p addr=%u", device,
                 new_flow, new_flow->addr);
        goto pcc_sched_cleanup;
    }

    LOG_DBG("[dev=%p] started thread for flow=%p addr=%u", device, new_flow,
            new_flow->addr);

    while (flow_state_get(new_flow) != FLOW_STATE_RUNNING) {
        ;
        /* wait for the new flow to initialize (arm triggers) */
    }

    if (device_scheduler_flow_add(&device->scheduler, new_flow)) {
        LOG_DBG("[dev=%p] failed to add flow=%p, addr=%u to scheduler", device,
                new_flow, new_flow->addr);
        goto free_flow;
    }

    *flow = new_flow;

    return SUCCESS;

pcc_sched_cleanup:
    device_scheduler_flow_remove(&device->scheduler, new_flow);
free_flow:
    free(new_flow);
    return ERROR;
}

int flow_destroy(flow_t *flow) {
    int ret = SUCCESS;

    if (device_scheduler_flow_remove(&flow->config->device->scheduler, flow)) {
        LOG_CRIT("[flow=%p addr=%u] failed to remove flow from scheduler", flow,
                 flow->addr);
        ret = ERROR;
    }

    flow->progress_state = FLOW_STATE_STOP;
    if (pthread_join(flow->thread, NULL)) {
        LOG_CRIT("[flow=%p addr=%u] flow thread join failed", flow, flow->addr);
        ret = ERROR;
    }

    if (flow_error_status_get(flow) != SUCCESS) {
        LOG_CRIT("[flow=%p addr=%u] flow thread completed with error", flow,
                 flow->addr);
        ret = ERROR;
    }

    free(flow);

    return ret;
}

void flow_signal_triggers_rearm(flow_t *flow) {
    const struct algorithm_config *config = flow->config;
    struct slist_entry *item, *prev;
    slist_foreach(&config->signals_list, item, prev) {
        (void)prev; /* suppress compiler warnings */
        struct signal_attr *attr =
            container_of(item, struct signal_attr, metadata.list_entry);
        if (attr->is_trigger &&
            attr->trigger_rearm_fn != flow_signal_trigger_rearm_no_op) {
            LOG_INFO("[flow=%p, addr=%u] rearm signal_type=%s, "
                     "accum_type=%s, index=%zu",
                     flow, flow->addr, signal_type_to_string(attr->type),
                     signal_accum_type_to_string(attr->accum_type),
                     attr->metadata.index);
            attr->trigger_rearm_fn(flow, attr);
        }
    }
}

bool flow_signal_triggers_check(flow_t *flow) {
    const struct algorithm_config *config = flow->config;
    struct slist_entry *item, *prev;
    slist_foreach(&config->signals_list, item, prev) {
        (void)prev; /* suppress compiler warnings */
        struct signal_attr *attr =
            container_of(item, struct signal_attr, metadata.list_entry);
        if (attr->is_trigger && attr->trigger_check_fn(flow, attr)) {
            flow->trigger_user_index = attr->metadata.index;
            LOG_INFO("[flow=%p, addr=%u] trigger signal_type=%s, "
                     "accum_type=%s, index=%zu",
                     flow, flow->addr, signal_type_to_string(attr->type),
                     signal_accum_type_to_string(attr->accum_type),
                     attr->metadata.index);
            return true;
        }
    }
    return false;
}

void flow_signals_update(flow_t *flow, signal_t signal_type, int value) {
    struct slist_entry *item, *prev;
    slist_foreach(&flow->config->signals_list, item, prev) {
        (void)prev; /* suppress compiler warnings */
        struct signal_attr *attr =
            container_of(item, struct signal_attr, metadata.list_entry);
        if (attr->type == signal_type &&
            attr->accumulation_op_fn != flow_signal_accumulation_no_op) {
            LOG_INFO("[flow=%p, addr=%u] update signal_type=%s, "
                     "accum_type=%s, index=%zu, update_val=%d",
                     flow, flow->addr, signal_type_to_string(signal_type),
                     signal_accum_type_to_string(attr->accum_type),
                     attr->metadata.index, value);
            attr->accumulation_op_fn(flow, attr, value);
        }
    }
}

bool flow_handler_invoke_on_trigger(flow_t *flow) {
    if (flow_signal_triggers_check(flow)) {
        flow_signals_update(flow, SIG_ELAPSED_TIME, 0);
        flow->config->algorithm_fn((void *)flow);
        flow_signal_triggers_rearm(flow);
        return true;
    }
    return false;
}

int flow_cwnd_get(const flow_t *flow) {
    size_t cwnd_idx;
    ATTR_LIST_FIRST_MATCH_BY_ATTR_TYPE_FIND(
        &flow->config->controls_list, struct control_attr, CTRL_CWND, cwnd_idx);
    return __flow_control_get(flow, cwnd_idx);
}

int flow_time_init(flow_t *flow) {
    // CLOCK_MONOTHONIC is system wide clock so it should be safe to share
    // it between flow and scheduler threads
    if (clock_gettime(CLOCK_MONOTONIC, &flow->start_ts))
        return ERROR;
    return SUCCESS;
}

void flow_state_set(flow_t *flow, flow_state_t new_state) {
    flow->progress_state = new_state;
}

flow_state_t flow_state_get(const flow_t *flow) { return flow->progress_state; }

void flow_error_status_set(flow_t *flow, int status) {
    flow->err_status = status;
}
int flow_error_status_get(const flow_t *flow) { return flow->err_status; }