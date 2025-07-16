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

pcm_uint flow_cwnd_get(const flow_t *flow) {
    size_t cwnd_idx;
    ATTR_LIST_FIRST_MATCH_BY_ATTR_TYPE_FIND(
        &flow->config->controls_list, struct control_attr, PCM_CTRL_CWND, cwnd_idx);
    return flow->device->flow_ops.handler.control_get(flow, cwnd_idx);
}

pcm_uint flow_time_get(const flow_t *flow) {
    return flow->device->flow_ops.control.time_get(flow);
}

bool flow_is_ready(const flow_t *flow) {
    return flow->device->flow_ops.control.is_ready(flow);
}

void flow_signals_update(flow_t *flow, pcm_signal_t signal_type, pcm_uint value) {
    struct slist_entry *item, *prev;
    slist_foreach(&flow->config->signals_list, item, prev) {
        (void)prev; /* suppress compiler warnings */
        struct signal_attr *attr =
            container_of(item, struct signal_attr, metadata.list_entry);
        if (attr->type == signal_type &&
            attr->accumulation_op_fn != flow_signal_accumulation_no_op) {
            LOG_DBG("[flow=%p, addr=%u] update signal_type=%s, "
                    "accum_type=%s, index=%zu, update_val=%d",
                    flow, flow->addr, signal_type_to_string(signal_type),
                    signal_accum_type_to_string(attr->accum_type),
                    attr->metadata.index, value);
            attr->accumulation_op_fn(flow, attr, value);
        }
    }
}

bool flow_triggers_check(flow_t *flow) {
    const struct algorithm_config *config = flow->config;

    for (size_t idx = 0; idx < config->num_signals; idx++) {
        bool trigger = false;
        struct signal_attr *attr = container_of(
            flow->cur_trigger, struct signal_attr, metadata.list_entry);
        if (attr->is_trigger && attr->trigger_check_fn(flow, attr)) {
            flow->trigger_user_index = attr->metadata.index;
            LOG_DBG("[flow=%p, addr=%u] trigger signal_type=%s, "
                    "accum_type=%s, index=%zu",
                    flow, flow->addr, signal_type_to_string(attr->type),
                    signal_accum_type_to_string(attr->accum_type),
                    attr->metadata.index);
            trigger = true;
        }

        if (flow->cur_trigger == config->signals_list.tail)
            flow->cur_trigger = config->signals_list.head;
        else
            flow->cur_trigger = flow->cur_trigger->next;

        if (trigger) {
            return trigger;
        }
    }

    return false;
}

void flow_triggers_arm(flow_t *flow) {
    const struct algorithm_config *config = flow->config;
    struct slist_entry *item, *prev;
    slist_foreach(&config->signals_list, item, prev) {
        (void)prev; /* suppress compiler warnings */
        struct signal_attr *attr =
            container_of(item, struct signal_attr, metadata.list_entry);
        if (attr->is_trigger &&
            attr->trigger_arm_fn != flow_signal_trigger_arm_no_op) {
            LOG_DBG("[flow=%p, addr=%u] rearm signal_type=%s, "
                    "accum_type=%s, index=%zu",
                    flow, flow->addr, signal_type_to_string(attr->type),
                    signal_accum_type_to_string(attr->accum_type),
                    attr->metadata.index);
            attr->trigger_arm_fn(flow, attr);
        }
    }
}

bool flow_handler_invoke_on_trigger(flow_t *flow) {
    PERF_PROF_REGION_SCOPE_INIT();
    PERF_PROF_REGION_START();
    bool handler_invoked = false;
    if (flow_triggers_check(flow)) {
        flow_signals_update(flow, PCM_SIG_ELAPSED_TIME, 0);
        flow->config->algorithm_fn((void *)flow, flow->signals,
                                   flow->thresholds, flow->controls,
                                   flow->local_state, flow->constants);
        LOG_DBG("[flow=%p addr=%u] time=%d cwnd=%d", flow, flow->addr,
                flow_time_get(flow), flow_cwnd_get(flow));
        flow_triggers_arm(flow);
        handler_invoked = true;
    }
    PERF_PROF_REGION_END(handler_invoked, "HANDLER PERFORMANCE");
    return handler_invoked;
}

int flow_destroy(flow_t *flow) {
    int ret = PCM_SUCCESS;

    if (device_scheduler_flow_remove(&flow->config->device->scheduler, flow)) {
        LOG_CRIT("[flow=%p addr=%u] failed to remove flow from scheduler", flow,
                 flow->addr);
        ret = PCM_ERROR;
    }

    if (flow->device->flow_ops.control.destroy(flow)) {
        LOG_CRIT("[flow=%p addr=%u] failed to destroy flow backend state", flow,
                 flow->addr)
        ret = PCM_ERROR;
    }

    free(flow);

    return ret;
}

int flow_create(device_t *device, flow_t **flow,
                traffic_gen_fn_t traffic_gen_fn) {
    if (!device)
        return PCM_ERROR;

    flow_t *new_flow = (flow_t *)calloc(
        1, sizeof(*new_flow) + device->flow_ops.control.max_regfile_size_get());
    if (!new_flow) {
        LOG_CRIT("[dev=%p] failed to allocate new flow", device);
        return PCM_ERROR;
    }

    new_flow->device = device;
    new_flow->addr = device->flow_addr_counter++;
    LOG_DBG("[dev=%p] allocated new flow=%p, addr=%u", device, new_flow,
            new_flow->addr);

    new_flow->config = device_flow_id_to_config_match(device, new_flow->addr);
    if (!new_flow->config) {
        LOG_CRIT("[dev=%p] failed to match flow addr=%u to algorithm config",
                 device, new_flow->addr);
        goto err_free_flow;
    }

    if (new_flow->device->flow_ops.control.create(new_flow, traffic_gen_fn)) {
        LOG_DBG("[dev=%p] failed to initialize backend state for flow addr=%u",
                device, new_flow->addr);
        goto err_free_flow;
    }

    new_flow->cur_trigger = new_flow->config->signals_list.head;

    if (device_scheduler_flow_add(&device->scheduler, new_flow)) {
        LOG_DBG("[dev=%p] failed to add flow=%p, addr=%u to scheduler", device,
                new_flow, new_flow->addr);
        goto err_destroy_plugin;
    }

    *flow = new_flow;

    return PCM_SUCCESS;

err_destroy_plugin:
    new_flow->device->flow_ops.control.destroy(new_flow);
err_free_flow:
    free(new_flow);
    return PCM_ERROR;
}
