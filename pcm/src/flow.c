#include <unistd.h>

#include "impl.h"
#include "util.h"

// Forward declarations
static void *flow_default_traffic_gen_fn(void *arg);

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

    new_flow->thread_state = FLOW_THREAD_INIT;
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

    while (new_flow->thread_state != FLOW_THREAD_RUNNING) {
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

    flow->thread_state = FLOW_THREAD_STOP;
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

static int flow_cwnd_get(const flow_t *flow) {
    size_t cwnd_idx;
    ATTR_LIST_FIRST_MATCH_BY_ATTR_TYPE_FIND(
        &flow->config->controls_list, struct control_attr, CTRL_CWND, cwnd_idx);
    return __flow_control_get(flow, cwnd_idx);
}

static int flow_time_init(flow_t *flow) {
    // CLOCK_MONOTHONIC is system wide clock so it should be safe to share
    // it between flow and scheduler threads
    if (clock_gettime(CLOCK_MONOTONIC, &flow->start_ts))
        return ERROR;
    return SUCCESS;
}

// Flow thread: emulate bandwidth, drops, NACKs, RTOs
static void *flow_default_traffic_gen_fn(void *arg) {
    flow_t *flow = arg;

    LOG_PRINT("[flow=%p, addr=%u] traffic generation started", flow,
              flow->addr);

    unsigned int rnd = (unsigned int)time(NULL);
    const int pkts_per_ms = (TGEN_BANDWIDTH_BPS / 8 / 1000) / TGEN_PACKET_SIZE;

    // Initialize time related signals of flow before traffic generation
    // starts
    if (flow_time_init(flow) != SUCCESS) {
        flow->status = ERROR;
        flow->thread_state = FLOW_THREAD_STOP;
        goto flow_thread_join;
    }

    // At that point flow is not yet added to the scheduler, so
    // all flow triggers need to be manually armed
    flow_signal_triggers_rearm(flow);

    // Signal that flow is ready to be added to the scheduler
    flow->thread_state = FLOW_THREAD_RUNNING;

    while (flow->thread_state == FLOW_THREAD_RUNNING) {
        int cwnd = flow_cwnd_get(flow);
        if (cwnd == 0) {
            usleep(TGEN_THREAD_SLEEP_TIME_US);
            continue;
        }
        int to_send = cwnd < pkts_per_ms ? cwnd : pkts_per_ms;
        flow_signals_update(flow, SIG_DATA_TX, to_send * TGEN_MSS);
        double roll = (double)rand_r(&rnd) / RAND_MAX;
        if (roll < TGEN_DROP_PROB) {
            flow_signals_update(flow, SIG_RTO, 1);
        } else if (roll < TGEN_DROP_PROB + TGEN_NACK_PROB) {
            flow_signals_update(flow, SIG_NACK, 1);
        } else {
            flow_signals_update(flow, SIG_RTT, TGEN_RTT);
            flow_signals_update(flow, SIG_ACK, 1);
            if (roll < TGEN_ECN_CONG_PROB) {
                flow_signals_update(flow, SIG_ECN, 1);
            }
        }
        usleep(TGEN_THREAD_SLEEP_TIME_US);
        LOG_PRINT("[flow=%p, addr=%u] traffic generator stats: "
                  "pkts_per_ms=%d, to_send=%d, cwnd=%d",
                  flow, flow->addr, pkts_per_ms, to_send, cwnd);
    }

    flow->status = SUCCESS;
flow_thread_join:
    LOG_PRINT("[flow=%p, addr=%u] traffic generation terminated with "
              "status=%d",
              flow, flow->addr, flow->status);
    return NULL;
}