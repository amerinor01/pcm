#include <unistd.h>

#include "impl.h"
#include "util.h"

// Forward declarations
static void *flow_default_traffic_gen_fn(void *arg);

void flow_state_init(const struct algorithm_config *config, flow_t *flow) {
    ATTR_LIST_FLOW_STATE_INIT(&config->signals_list, struct signal_attr,
                              flow->datapath_state,
                              FLOW_SIGNALS_THRESHOLDS_OFFSET, ATOMIC_STORE);
    ATTR_LIST_FLOW_STATE_INIT(&config->controls_list, struct control_attr,
                              flow->datapath_state, FLOW_CONTROLS_OFFSET,
                              ATOMIC_STORE);
    ATTR_LIST_FLOW_STATE_INIT(&config->local_state_list,
                              struct local_state_attr, flow->local_state,
                              FLOW_LOCAL_STATE_VARS_OFFSET, NOSYNC_STORE);
    LOG_DBG("[dev=%p conf=%p] instantiated config on flow=%p id=%d",
            config->device, config, flow, flow->id);
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

    new_flow->id = device->flow_id_counter++;
    LOG_DBG("[dev=%p] allocated new flow=%p, id=%u", device, new_flow,
            new_flow->id);

    new_flow->config = device_flow_id_to_config_match(device, new_flow->id);
    if (!new_flow->config) {
        LOG_CRIT("[dev=%p] failed to match flow id=%u to algorithm config",
                 device, new_flow->id);
        goto free_flow;
    }

    flow_state_init(new_flow->config, new_flow);

    if (device_scheduler_flow_add(&device->scheduler, new_flow)) {
        LOG_DBG("[dev=%p] failed to add flow=%p, id=%u to scheduler", device,
                new_flow, new_flow->id);
        goto free_flow;
    }

    atomic_store(&new_flow->running, true);
    if (pthread_create(&new_flow->thread, NULL,
                       traffic_gen_fn ? traffic_gen_fn
                                      : flow_default_traffic_gen_fn,
                       (void *)new_flow)) {
        LOG_CRIT("[dev=%p] failed to start thread for flow=%p id=%u", device,
                 new_flow, new_flow->id);
        goto pcc_sched_cleanup;
    }

    LOG_DBG("[dev=%p] started thread for flow=%p id=%u", device, new_flow,
            new_flow->id);

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
        LOG_CRIT("[flow=%p id=%u] failed to remove flow from scheduler", flow,
                 flow->id);
        ret = ERROR;
    }

    atomic_store(&flow->running, false);
    if (pthread_join(flow->thread, NULL)) {
        LOG_CRIT("[flow=%p id=%u] flow thread join failed", flow, flow->id);
        ret = ERROR;
    }

    if (flow->status != SUCCESS) {
        LOG_CRIT("[flow=%p id=%u] flow thread completed with error", flow,
                 flow->id);
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
        if (attr->is_trigger && attr->trigger_check_fn(attr, flow)) {
            return true;
        }
    }
    return false;
}

// Flow thread: emulate bandwidth, drops, NACKs, RTOs
static void *flow_default_traffic_gen_fn(void *arg) {
    flow_t *flow = arg;

    uint64_t tid;
    if (pthread_threadid_np(NULL, &tid)) {
        flow->status = ERROR;
        goto thread_termination;
    }

    // lookup signal indices
    size_t acks_idx, rto_idx, nacks_idx, cwnd_idx;
    ATTR_LIST_FIRST_MATCH_BY_ATTR_TYPE_FIND(
        &flow->config->signals_list, struct signal_attr, SIG_ACK, acks_idx);
    ATTR_LIST_FIRST_MATCH_BY_ATTR_TYPE_FIND(
        &flow->config->signals_list, struct signal_attr, SIG_RTO, rto_idx);
    ATTR_LIST_FIRST_MATCH_BY_ATTR_TYPE_FIND(
        &flow->config->signals_list, struct signal_attr, SIG_NACK, nacks_idx);
    ATTR_LIST_FIRST_MATCH_BY_ATTR_TYPE_FIND(
        &flow->config->controls_list, struct control_attr, CTRL_CWND, cwnd_idx);
    if (acks_idx == SIZE_MAX || rto_idx == SIZE_MAX || nacks_idx == SIZE_MAX ||
        cwnd_idx == SIZE_MAX) {
        flow->status = ERROR;
        goto thread_termination;
    }
    acks_idx += FLOW_SIGNALS_OFFSET;
    rto_idx += FLOW_SIGNALS_OFFSET;
    nacks_idx += FLOW_SIGNALS_OFFSET;
    cwnd_idx += FLOW_CONTROLS_OFFSET;
    LOG_PRINT("[tid=%llu, flow=%p, id=%u] traffic generation started; "
              "acks_idx=%zu, rto_idx=%zu, nacks_idx=%zu, "
              "cwnd_idx=%zu",
              tid, flow, flow->id, acks_idx, rto_idx, nacks_idx, cwnd_idx);

    unsigned int rnd = (unsigned int)time(NULL);
    const int pkts_per_ms = (TGEN_BANDWIDTH_BPS / 8 / 1000) / TGEN_PACKET_SIZE;
    while (atomic_load(&flow->running)) {
        int cwnd = __flow_control_get(flow, 0);
        if (cwnd == 0) {
            usleep(TGEN_THREAD_SLEEP_TIME_US);
            continue;
        }
        int to_send = cwnd < pkts_per_ms ? cwnd : pkts_per_ms;
        double roll = (double)rand_r(&rnd) / RAND_MAX;
        if (roll < TGEN_DROP_PROB) {
            atomic_fetch_add(&flow->datapath_state[rto_idx], 1);
        } else if (roll < TGEN_DROP_PROB + TGEN_NACK_PROB) {
            atomic_fetch_add(&flow->datapath_state[nacks_idx], 1);
        } else {
            atomic_fetch_add(&flow->datapath_state[acks_idx], to_send);
        }
        usleep(TGEN_THREAD_SLEEP_TIME_US);
        LOG_PRINT("[tid=%llu, flow=%p, id=%u] traffic generator stats: "
                  "pkts_per_ms=%d, to_send=%d, cwnd=%d",
                  tid, flow, flow->id, pkts_per_ms, to_send, cwnd);
    }

    flow->status = SUCCESS;

thread_termination:
    LOG_PRINT("[tid=%llu flow=%p, id=%u] traffic generation terminated with "
              "status=%d",
              tid, flow, flow->id, flow->status);
    return NULL;
}