#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "impl.h"
#include "prof.h"
#include "util.h"

signal_trigger_arm_fn flow_signal_trigger_arm_no_op = NULL;
signal_accumulation_op_fn flow_signal_accumulation_no_op = NULL;

pcm_uint flow_cwnd_get(const pcm_flow_t flow) {
    size_t cwnd_idx;
    ATTR_LIST_FIRST_MATCH_BY_ATTR_TYPE_FIND(&flow->config->controls_list,
                                            struct control_attr, PCM_CTRL_CWND,
                                            cwnd_idx);
    return flow->device->flow_ops.datapath.control_get(flow, cwnd_idx);
}

void flow_cwnd_set(const pcm_flow_t flow, pcm_uint cwnd) {
    size_t cwnd_idx;
    ATTR_LIST_FIRST_MATCH_BY_ATTR_TYPE_FIND(&flow->config->controls_list,
                                            struct control_attr, PCM_CTRL_CWND,
                                            cwnd_idx);
    return flow->device->flow_ops.datapath.control_set(flow, cwnd_idx, cwnd);
}

pcm_uint flow_time_get(const pcm_flow_t flow) {
    return flow->device->flow_ops.control.time_get(flow);
}

bool flow_is_ready(const pcm_flow_t flow) {
    return flow->device->flow_ops.control.is_ready(flow);
}

static void flow_datapath_snapshot_prepare(pcm_flow_t flow) {
    flow->device->flow_ops.datapath.snapshot_prepare(flow);
}

static void flow_datapath_snapshot_apply(pcm_flow_t flow) {
    flow->device->flow_ops.datapath.snapshot_apply(flow);
}

void flow_signals_update(pcm_flow_t flow, pcm_signal_t signal_type,
                         pcm_uint value) {
    struct slist_entry *item, *prev;
    slist_foreach(&flow->config->signals_list, item, prev) {
        (void)prev; /* suppress compiler warnings */
        struct signal_attr *attr =
            container_of(item, struct signal_attr, metadata.list_entry);
        if (attr->type == signal_type &&
            attr->accumulation_op_fn != flow_signal_accumulation_no_op) {
            PCM_LOG_DBG("[flow=%p, addr=%u] update signal_type=%s, "
                        "accum_type=%s, idx=%zu, update_val=%d",
                        flow, flow->addr, signal_type_to_string(signal_type),
                        signal_accum_type_to_string(attr->accum_type),
                        UTIL_MASK_TO_ARR_IDX(attr->metadata.idx), value);
            attr->accumulation_op_fn(flow, attr, value);
        }
    }
}

static bool flow_triggers_check(pcm_flow_t flow) {
    const struct algorithm_config *config = flow->config;
    // mask should be fresh from previous invocation, otherwise handler
    // invocation teardown logic is broken!
    if (flow->datapath_snapshot.trigger_mask)
        PCM_LOG_FATAL(
            "[flow=%p, addr=%u] datapath_snapshot.trigger_mask is not empty!",
            flow, flow->addr);
    bool trigger = false;
    struct slist_entry *item, *prev;
    slist_foreach(&config->signals_list, item, prev) {
        (void)prev;
        struct signal_attr *attr =
            container_of(item, struct signal_attr, metadata.list_entry);
        if (attr->is_trigger && attr->trigger_check_fn(flow, attr)) {
            flow->datapath_snapshot.trigger_mask |= attr->metadata.idx;
            PCM_LOG_DBG("[flow=%p, addr=%u] trigger signal_type=%s, "
                        "accum_type=%s, idx=%zu",
                        flow, flow->addr, signal_type_to_string(attr->type),
                        signal_accum_type_to_string(attr->accum_type),
                        UTIL_MASK_TO_ARR_IDX(attr->metadata.idx));
            if (!trigger) {
                trigger = true;
            }
        } else if (attr->type == PCM_SIG_ELAPSED_TIME &&
                   attr->accumulation_op_fn != flow_signal_accumulation_no_op) {
            attr->accumulation_op_fn(flow, attr, 0); // progress time
        }
    }
    if (trigger)
        return trigger;
    return false;
}

void flow_triggers_arm(pcm_flow_t flow) {
    const struct algorithm_config *config = flow->config;
    struct slist_entry *item, *prev;
    slist_foreach(&config->signals_list, item, prev) {
        (void)prev; /* suppress compiler warnings */
        struct signal_attr *attr =
            container_of(item, struct signal_attr, metadata.list_entry);
        if (attr->is_trigger &&
            attr->trigger_arm_fn != flow_signal_trigger_arm_no_op) {
            PCM_LOG_DBG("[flow=%p, addr=%u] rearm signal_type=%s, "
                        "accum_type=%s, idx=%zu",
                        flow, flow->addr, signal_type_to_string(attr->type),
                        signal_accum_type_to_string(attr->accum_type),
                        UTIL_MASK_TO_ARR_IDX(attr->metadata.idx));
            attr->trigger_arm_fn(flow, attr);
        }
    }
    flow->datapath_snapshot.trigger_mask = 0;
}

bool flow_handler_invoke_on_trigger(pcm_flow_t flow) {
    PCM_PERF_PROF_REGION_SCOPE_INIT(trigger_cycle, "TRIGGER CYCLE");
    bool invoke;
    PCM_PERF_PROF_REGION_START(trigger_cycle);
    if ((invoke = flow_triggers_check(flow))) {
        flow_datapath_snapshot_prepare(flow);
        flow->config->algorithm_fn(&flow->datapath_snapshot);
        flow_datapath_snapshot_apply(flow);
        flow_triggers_arm(flow);
        PCM_LOG_DBG("[flow=%p addr=%u] time=%d cwnd=%d", flow, flow->addr,
                    flow_time_get(flow), flow_cwnd_get(flow));
    }
    PCM_PERF_PROF_REGION_END(trigger_cycle, invoke);
    return invoke;
}

pcm_err_t flow_destroy(pcm_flow_t flow) {
    pcm_err_t ret = PCM_SUCCESS;

    if (device_scheduler_flow_remove(&flow->config->device->scheduler, flow)) {
        PCM_LOG_CRIT("[flow=%p addr=%u] failed to remove flow from scheduler",
                     flow, flow->addr);
        ret = PCM_ERROR;
    }

    if (flow->device->flow_ops.control.destroy(flow)) {
        PCM_LOG_CRIT("[flow=%p addr=%u] failed to destroy flow backend state",
                     flow, flow->addr)
        ret = PCM_ERROR;
    }

    PCM_LOG_INFO("[dev=%p config=%p] flow=%p destroyed", flow->config->device,
                 flow->config, flow);

    free(flow);

    return ret;
}

pcm_err_t flow_create(pcm_device_t device, pcm_flow_t *flow,
                      traffic_gen_fn_t traffic_gen_fn) {
    if (!device)
        return PCM_ERROR;

    pcm_flow_t new_flow = (pcm_flow_t)calloc(1, sizeof(*new_flow));
    if (!new_flow) {
        PCM_LOG_CRIT("[dev=%p] failed to allocate new flow", device);
        return PCM_ERROR;
    }

    new_flow->device = device;
    new_flow->addr = device->flow_addr_counter++;
    PCM_LOG_DBG("[dev=%p] allocated new flow=%p, addr=%u", device, new_flow,
                new_flow->addr);

    new_flow->config = device_flow_id_to_config_match(device, new_flow->addr);
    if (!new_flow->config) {
        PCM_LOG_CRIT(
            "[dev=%p] failed to match flow addr=%u to algorithm config", device,
            new_flow->addr);
        goto err_free_flow;
    }

    if (new_flow->device->flow_ops.control.create(new_flow, traffic_gen_fn)) {
        PCM_LOG_DBG(
            "[dev=%p] failed to initialize backend state for flow addr=%u",
            device, new_flow->addr);
        goto err_free_flow;
    }

    // Variables are local to the handler, so can be stored directly in the
    // snapshot
    ATTR_LIST_FLOW_STATE_INIT(&new_flow->config->var_list, struct var_attr,
                              new_flow->datapath_snapshot.vars, false);

    if (device_scheduler_flow_add(&device->scheduler, new_flow)) {
        PCM_LOG_DBG("[dev=%p] failed to add flow=%p, addr=%u to scheduler",
                    device, new_flow, new_flow->addr);
        goto err_destroy_plugin;
    }

    PCM_LOG_INFO("[dev=%p config=%p] flow=%p created", device, new_flow->config,
                 new_flow);

    *flow = new_flow;

    return PCM_SUCCESS;

err_destroy_plugin:
    new_flow->device->flow_ops.control.destroy(new_flow);
err_free_flow:
    free(new_flow);
    return PCM_ERROR;
}
