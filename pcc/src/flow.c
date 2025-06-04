#include <unistd.h>

#include "impl.h"
#include "util.h"

// Forward declarations
static void *flow_default_traffic_gen_fn(void *arg);

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

bool flow_handler_trigger_check(const flow_t *flow,
                                const struct signal_attr *attr) {
    int value =
        flow->datapath_state[FLOW_SIGNALS_OFFSET + attr->metadata.index];
    int threshold = flow->datapath_state[FLOW_SIGNALS_THRESHOLDS_OFFSET +
                                         attr->metadata.index];
    if (value >= threshold)
        return true;
    return false;
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

    if (device_scheduler_flow_add(&device->scheduler, new_flow)) {
        LOG_DBG("[dev=%p] failed to add flow=%p, addr=%u to scheduler", device,
                new_flow, new_flow->addr);
        goto free_flow;
    }

    new_flow->running = true;
    if (pthread_create(&new_flow->thread, NULL,
                       traffic_gen_fn ? traffic_gen_fn
                                      : flow_default_traffic_gen_fn,
                       (void *)new_flow)) {
        LOG_CRIT("[dev=%p] failed to start thread for flow=%p addr=%u", device,
                 new_flow, new_flow->addr);
        goto pcc_sched_cleanup;
    }

    LOG_DBG("[dev=%p] started thread for flow=%p addr=%u", device, new_flow,
            new_flow->addr);

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

    flow->running = false;
    if (pthread_join(flow->thread, NULL)) {
        LOG_CRIT("[flow=%p addr=%u] flow thread join failed", flow, flow->addr);
        ret = ERROR;
    }

    if (flow->status != SUCCESS) {
        LOG_CRIT("[flow=%p addr=%u] flow thread completed with error", flow,
                 flow->addr);
        ret = ERROR;
    }

    free(flow);

    return ret;
}

bool flow_triggers_check(const flow_t *flow) {
    const struct algorithm_config *config = flow->config;
    struct slist_entry *item, *prev;
    slist_foreach(&config->signals_list, item, prev) {
        struct signal_attr *attr =
            container_of(item, struct signal_attr, metadata.list_entry);
        if (attr->is_trigger && attr->trigger_check_fn(flow, attr)) {
            return true;
        }
    }
    return false;
}

static void flow_signals_update(flow_t *flow, signal_t signal_type, int value) {
    struct slist_entry *item, *prev;
    slist_foreach(&flow->config->signals_list, item, prev) {
        struct signal_attr *attr =
            container_of(item, struct signal_attr, metadata.list_entry);
        if (attr->type == signal_type) {
            attr->accumulation_op_fn(flow, attr, value);
        }
    }
}

static int flow_cwnd_get(const flow_t *flow) {
    size_t cwnd_idx;
    ATTR_LIST_FIRST_MATCH_BY_ATTR_TYPE_FIND(
        &flow->config->controls_list, struct control_attr, CTRL_CWND, cwnd_idx);
    return __flow_control_get(flow, cwnd_idx);
}

// Flow thread: emulate bandwidth, drops, NACKs, RTOs
static void *flow_default_traffic_gen_fn(void *arg) {
    flow_t *flow = arg;

    uint64_t tid;
    if (pthread_threadid_np(NULL, &tid)) {
        flow->status = ERROR;
        goto thread_termination;
    }

    LOG_PRINT("[tid=%llu, flow=%p, addr=%u] traffic generation started; ", tid,
              flow, flow->addr);

    unsigned int rnd = (unsigned int)time(NULL);
    const int pkts_per_ms = (TGEN_BANDWIDTH_BPS / 8 / 1000) / TGEN_PACKET_SIZE;
    while (flow->running) {
        int cwnd = flow_cwnd_get(flow);
        if (cwnd == 0) {
             usleep(TGEN_THREAD_SLEEP_TIME_US);
             continue;
        }
        int to_send = cwnd < pkts_per_ms ? cwnd : pkts_per_ms;
        double roll = (double)rand_r(&rnd) / RAND_MAX;
        if (roll < TGEN_DROP_PROB) {
            flow_signals_update(flow, SIG_RTO, 1);
        } else if (roll < TGEN_DROP_PROB + TGEN_NACK_PROB) {
            flow_signals_update(flow, SIG_NACK, 1);
        } else {
            flow_signals_update(flow, SIG_ACK, 1);
        }
        usleep(TGEN_THREAD_SLEEP_TIME_US);
        LOG_PRINT("[tid=%llu, flow=%p, addr=%u] traffic generator stats: "
                  "pkts_per_ms=%d, to_send=%d, cwnd=%d",
                  tid, flow, flow->addr, pkts_per_ms, to_send, cwnd);
    }

    flow->status = SUCCESS;

thread_termination:
    LOG_PRINT("[tid=%llu flow=%p, addr=%u] traffic generation terminated with "
              "status=%d",
              tid, flow, flow->addr, flow->status);
    return NULL;
}