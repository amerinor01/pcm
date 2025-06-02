#include <dlfcn.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "driver.h"

#define container_of(ptr, type, member)                                        \
    ({                                                                         \
        const typeof(((type *)0)->member) *__mptr = (ptr);                     \
        (type *)((char *)__mptr - offsetof(type, member));                     \
    })

#define ATOMIC_STORE(dst, val) (atomic_store(&dst, val))
#define NOSYNC_STORE(dst, val) (dst = val)

#define ATTR_LIST_ITEM_ALLOC(attr_list, user_index, item_counter, max_items,   \
                             attr_ptr)                                         \
    {                                                                          \
        if ((item_counter) >= (max_items)) {                                   \
            return ERROR;                                                      \
        }                                                                      \
        if ((user_index) >= (max_items)) {                                     \
            return ERROR;                                                      \
        }                                                                      \
        (attr_ptr) = calloc(1, sizeof(*(attr_ptr)));                           \
        if (!(attr_ptr)) {                                                     \
            return ERROR;                                                      \
        }                                                                      \
        (attr_ptr)->metadata.index = (user_index);                             \
        slist_insert_head(&(attr_ptr)->metadata.list_entry, (attr_list));      \
        ++(item_counter);                                                      \
    }

#define ATTR_LIST_FREE(attr_list, attr_type, item_counter)                     \
    if (item_counter) {                                                        \
        while (!slist_empty(attr_list)) {                                      \
            struct slist_entry *entry = slist_remove_head(attr_list);          \
            attr_type *attr =                                                  \
                container_of(entry, attr_type, metadata.list_entry);           \
            free(attr);                                                        \
            --(item_counter);                                                  \
        }                                                                      \
    }

#define ATTR_LIST_DUPLICATE_USER_INDEX_CHK(attr_list, attr_type, user_index)   \
    {                                                                          \
        struct slist_entry *item, *prev;                                       \
        slist_foreach(attr_list, item, prev) {                                 \
            if (container_of(item, attr_type, metadata.list_entry)             \
                    ->metadata.index == (user_index)) {                        \
                return ERROR;                                                  \
            }                                                                  \
        }                                                                      \
    }

#define ATTR_LIST_FIRST_MATCH_BY_ATTR_TYPE_FIND(attr_list, attr_type,          \
                                                entry_type, found_idx)         \
    {                                                                          \
        struct slist_entry *item, *prev;                                       \
        (found_idx) = SIZE_MAX;                                                \
        slist_foreach(attr_list, item, prev) {                                 \
            attr_type *attr =                                                  \
                container_of(item, attr_type, metadata.list_entry);            \
            if (attr->type == (entry_type)) {                                  \
                (found_idx) = attr->metadata.index;                            \
                break;                                                         \
            }                                                                  \
        }                                                                      \
    }

#define ATTR_LIST_ITEM_SET(attr_list, attr_type, user_index, val,              \
                           found_attr_ptr)                                     \
    {                                                                          \
        if (slist_empty(attr_list)) {                                          \
            return ERROR;                                                      \
        }                                                                      \
        (found_attr_ptr) = NULL;                                               \
        attr_type *cur_attr = NULL;                                            \
        struct slist_entry *item, *prev;                                       \
        slist_foreach(attr_list, item, prev) {                                 \
            cur_attr = container_of(item, attr_type, metadata.list_entry);     \
            if (cur_attr->metadata.index == (user_index)) {                    \
                (found_attr_ptr) = cur_attr;                                   \
                break;                                                         \
            }                                                                  \
        }                                                                      \
        if (!(found_attr_ptr)) {                                               \
            return ERROR;                                                      \
        }                                                                      \
        (found_attr_ptr)->metadata.value = val;                                \
    }

#define ATTR_LIST_FLOW_STATE_INIT(attr_list, attr_type, state_ptr,             \
                                  state_offset, store_fn)                      \
    {                                                                          \
        attr_type *cur_attr = NULL;                                            \
        struct slist_entry *item, *prev;                                       \
        slist_foreach(attr_list, item, prev) {                                 \
            cur_attr = container_of(item, attr_type, metadata.list_entry);     \
            store_fn((state_ptr)[(state_offset) + cur_attr->metadata.index],   \
                     cur_attr->metadata.value);                                \
        }                                                                      \
    }

// Forward declarations
static void *flow_default_traffic_gen_fn(void *arg);
static void *scheduler_thread_fn(void *arg);
void algorithm_config_flow_state_init(const struct algorithm_config *config,
                                      flow_t *flow);
bool flow_triggers_check(const flow_t *flow);

int scheduler_init(struct scheduler *scheduler) {
    if (pthread_mutex_init(&scheduler->flow_list_lock, NULL))
        return ERROR;
    slist_init(&scheduler->flow_list);

    scheduler->status = SUCCESS;
    atomic_init(&scheduler->running, true);
    if (pthread_create(&scheduler->thread, NULL, scheduler_thread_fn,
                       (void *)scheduler))
        goto err;

    return SUCCESS;

err:
    scheduler->status = ERROR;
    atomic_init(&scheduler->running, false);
    pthread_mutex_destroy(&scheduler->flow_list_lock);
    return ERROR;
}

int scheduler_destroy(struct scheduler *scheduler) {
    int ret = SUCCESS;

    if (!atomic_load(&scheduler->running))
        return ERROR;

    atomic_store(&scheduler->running, false);
    if (pthread_join(scheduler->thread, NULL))
        ret = ERROR;

    if (scheduler->status)
        ret = ERROR;

    if (pthread_mutex_destroy(&scheduler->flow_list_lock))
        ret = ERROR;

    return ret;
}

int scheduler_flow_add(struct scheduler *scheduler, flow_t *flow) {
    if (pthread_mutex_lock(&scheduler->flow_list_lock))
        return ERROR;

    slist_insert_tail(&flow->flow_list_entry, &scheduler->flow_list);

    if (pthread_mutex_unlock(&scheduler->flow_list_lock))
        return ERROR;

    LOG_DBG("Added flow=%p, id=%u to scheduler=%p", flow, flow->flow_id,
            scheduler);
    return SUCCESS;
}

int scheduler_flow_remove(struct scheduler *scheduler, flow_t *flow) {
    if (pthread_mutex_lock(&scheduler->flow_list_lock))
        return ERROR;

    struct slist_entry *item, *prev;
    bool found = false;
    slist_foreach(&scheduler->flow_list, item, prev) {
        if (container_of(item, flow_t, flow_list_entry) == flow) {
            slist_remove(&scheduler->flow_list, item, prev);
            found = true;
            break;
        }
    }

    if (pthread_mutex_unlock(&scheduler->flow_list_lock))
        return ERROR;

    if (!found)
        return ERROR;

    return SUCCESS;
}

static void *scheduler_thread_fn(void *arg) {
    struct scheduler *scheduler = arg;

    while (atomic_load(&scheduler->running)) {
        if (pthread_mutex_lock(&scheduler->flow_list_lock)) {
            scheduler->status = ERROR;
            break;
        }

        struct slist_entry *item, *prev;
        slist_foreach(&scheduler->flow_list, item, prev) {
            flow_t *flow = container_of(item, flow_t, flow_list_entry);
            if (!atomic_load(&flow->running)) {
                scheduler->status = ERROR;
                break;
            }
            if (flow_triggers_check(flow)) {
                flow->config->algorithm_fn((void *)flow);
            }
        }

        if (pthread_mutex_unlock(&scheduler->flow_list_lock))
            scheduler->status = ERROR;

        if (scheduler->status == ERROR)
            break;

        usleep(SCHEDULER_SLEEP_US);
    }

    return NULL;
}

int device_init(const char *device_name, device_t **out) {
    (void)device_name;
    device_t *device = calloc(1, sizeof(*device));

    if (!device)
        return ERROR;

    slist_init(&device->configs_list);

    if (scheduler_init(&device->scheduler))
        goto err;

    *out = device;

    return SUCCESS;

err:
    free(device);

    return ERROR;
}

int device_destroy(device_t *device) {
    int ret = SUCCESS;

    ret = scheduler_destroy(&device->scheduler);

    if (!slist_empty(&device->configs_list))
        ret = ERROR;

    free(device);

    return ret;
}

const struct algorithm_config *device_flow_to_config_match(device_t *device,
                                                           int flow_id) {
    struct slist_entry *item, *prev;
    slist_foreach(&device->configs_list, item, prev) {
        struct algorithm_config *config =
            container_of(item, struct algorithm_config, list_entry);
        /*
         * Rather than doing real matching below, the code below mimics it with
         * logical OR.
         */
        if (config->active && (config->matching_rule_mask | flow_id)) {
            LOG_DBG("Matched id=%u to config=%p", flow_id, config);
            return config;
        }
    }
    return NULL;
}

int flow_create(device_t *device, flow_t **flow,
                traffic_gen_fn_t traffic_gen_fn) {
    if (!device)
        return ERROR;

    flow_t *new_flow = calloc(1, sizeof(*new_flow));
    if (!new_flow)
        return ERROR;
    new_flow->flow_id = device->flow_id_counter++;
    LOG_DBG("Allocated new flow=%p, id=%u", new_flow, new_flow->flow_id);

    new_flow->config = device_flow_to_config_match(device, new_flow->flow_id);
    if (!new_flow->config)
        goto free_flow;
    algorithm_config_flow_state_init(new_flow->config, new_flow);

    if (scheduler_flow_add(&device->scheduler, new_flow))
        goto free_flow;

    atomic_store(&new_flow->running, true);
    if (pthread_create(&new_flow->thread, NULL,
                       traffic_gen_fn ? traffic_gen_fn
                                      : flow_default_traffic_gen_fn,
                       (void *)new_flow))
        goto pcc_sched_cleanup;
    LOG_DBG("Started thread for flow=%p, id=%u", new_flow, new_flow->flow_id);

    *flow = new_flow;

    return SUCCESS;

pcc_sched_cleanup:
    scheduler_flow_remove(&device->scheduler, new_flow);
free_flow:
    free(new_flow);
    return ERROR;
}

int flow_destroy(flow_t *flow) {
    int ret = SUCCESS;

    if (scheduler_flow_remove(&flow->config->device->scheduler, flow))
        ret = ERROR;

    atomic_store(&flow->running, false);
    if (pthread_join(flow->thread, NULL))
        ret = ERROR;

    if (flow->status != SUCCESS)
        ret = ERROR;

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
    unsigned int rnd = (unsigned int)time(NULL);
    LOG_PRINT("[flow=%p, id=%d] start traffic generation", flow, flow->flow_id);

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
    LOG_PRINT("[flow=%p, id=%d] acks_idx=%zu, rto_idx=%zu, nacks_idx=%zu, "
              "cwnd_idx=%zu",
              flow, flow->flow_id, acks_idx, rto_idx, nacks_idx, cwnd_idx);

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
        usleep(TGEN_THREAD_SLEEP_TIME_US); // 1 ms
        LOG_PRINT("[flow=%p, id=%d] pkts_per_ms=%d, to_send=%d, cwnd=%d", flow,
                  flow->flow_id, pkts_per_ms, to_send, cwnd);
    }

    flow->status = SUCCESS;

thread_termination:
    LOG_PRINT("[flow=%p, id=%d] termination, status=%d", flow, flow->flow_id,
              flow->status);
    return NULL;
}

int algorithm_config_alloc(device_t *device, struct algorithm_config **config) {
    if (!device)
        return ERROR;

    struct algorithm_config *new_config = calloc(1, sizeof(*new_config));

    new_config->device = device;
    slist_insert_tail(&new_config->list_entry, &device->configs_list);
    slist_init(&new_config->signals_list);
    slist_init(&new_config->controls_list);
    slist_init(&new_config->local_state_list);
    new_config->active = false;
    *config = new_config;

    return SUCCESS;
}

int algorithm_config_destroy(struct algorithm_config *config) {
    int ret = SUCCESS;

    struct slist_entry *item, *prev;
    bool found = false;
    slist_foreach(&config->device->configs_list, item, prev) {
        if (container_of(item, struct algorithm_config, list_entry) == config) {
            slist_remove(&config->device->configs_list, item, prev);
            found = true;
        }
    }

    if (!found)
        ret = ERROR;

    ATTR_LIST_FREE(&config->signals_list, struct signal_attr,
                   config->num_signals);
    ATTR_LIST_FREE(&config->controls_list, struct control_attr,
                   config->num_controls);
    ATTR_LIST_FREE(&config->local_state_list, struct local_state_attr,
                   config->num_local_states);

    if (dlclose(config->dlopen_handle)) {
        LOG_CRIT("dlclose() failed: %s", dlerror());
        ret = ERROR;
    }

    free(config);

    return ret;
}

int algorithm_config_matching_rule_add(struct algorithm_config *config,
                                       int matching_rule_mask) {
    config->matching_rule_mask = matching_rule_mask;
    return SUCCESS;
}

int algorithm_config_compile(struct algorithm_config *config,
                             const char *compile_path, char **err) {
    config->dlopen_handle = dlopen(compile_path, RTLD_NOW | RTLD_LOCAL);
    if (!config->dlopen_handle) {
        *err = dlerror();
        return ERROR;
    }

    config->algorithm_fn = (algo_function_t)dlsym(
        config->dlopen_handle, __algorithm_entry_point_symbol);
    if (!config->algorithm_fn) {
        *err = dlerror();
        dlclose(config->dlopen_handle);
        return ERROR;
    }

    return SUCCESS;
}

int algorithm_config_activate(struct algorithm_config *config) {
    if (config->active)
        return ERROR;
    config->active = true;
    return SUCCESS;
}

int algorithm_config_deactivate(struct algorithm_config *config) {
    if (!config->active)
        return ERROR;
    config->active = false;
    return SUCCESS;
}

void algorithm_config_flow_state_init(const struct algorithm_config *config,
                                      flow_t *flow) {
    ATTR_LIST_FLOW_STATE_INIT(&config->signals_list, struct signal_attr,
                              flow->datapath_state,
                              FLOW_SIGNALS_THRESHOLDS_OFFSET, ATOMIC_STORE);
    ATTR_LIST_FLOW_STATE_INIT(&config->controls_list, struct control_attr,
                              flow->datapath_state, FLOW_CONTROLS_OFFSET,
                              ATOMIC_STORE);
    ATTR_LIST_FLOW_STATE_INIT(&config->local_state_list,
                              struct local_state_attr, flow->local_state,
                              FLOW_LOCAL_STATE_VARS_OFFSET, NOSYNC_STORE);
}

int algorithm_config_signal_add(struct algorithm_config *config,
                                signal_t signal, signal_accum_t accum_type,
                                size_t user_index) {
    ATTR_LIST_DUPLICATE_USER_INDEX_CHK(&config->signals_list,
                                       struct signal_attr, user_index);
    struct signal_attr *attr;
    ATTR_LIST_ITEM_ALLOC(&config->signals_list, user_index, config->num_signals,
                         ALGO_CONF_MAX_NUM_SIGNALS, attr);

    attr->type = signal;
    attr->accumulate_op = accum_type;

    return SUCCESS;
}

bool signal_trigger_threshold_check(const struct signal_attr *attr,
                                    const flow_t *flow) {

    int value = atomic_load(
        &flow->datapath_state[FLOW_SIGNALS_OFFSET + attr->metadata.index]);
    int threshold =
        atomic_load(&flow->datapath_state[FLOW_SIGNALS_THRESHOLDS_OFFSET +
                                          attr->metadata.index]);
    if (value >= threshold) {
        return true;
    }
    return false;
}

int algorithm_config_signal_trigger_set(struct algorithm_config *config,
                                        size_t user_index, int threshold) {
    struct signal_attr *attr;
    ATTR_LIST_ITEM_SET(&config->signals_list, struct signal_attr, user_index,
                       threshold, attr);
    if (threshold <= 0)
        return ERROR;

    attr->is_trigger = true;
    attr->trigger_check_fn = signal_trigger_threshold_check;
    return SUCCESS;
}

int algorithm_config_control_add(struct algorithm_config *config,
                                 control_t control, size_t user_index) {
    ATTR_LIST_DUPLICATE_USER_INDEX_CHK(&config->controls_list,
                                       struct control_attr, user_index);
    struct control_attr *attr;
    ATTR_LIST_ITEM_ALLOC(&config->controls_list, user_index,
                         config->num_controls, ALGO_CONF_MAX_NUM_CONTROLS,
                         attr);
    attr->type = control;
    return SUCCESS;
}

int algorithm_config_control_initial_value_set(struct algorithm_config *config,
                                               size_t user_index,
                                               int initial_value) {
    struct control_attr *attr;
    ATTR_LIST_ITEM_SET(&config->controls_list, struct control_attr, user_index,
                       initial_value, attr);
    return SUCCESS;
}

int algorithm_config_local_state_add(struct algorithm_config *config,
                                     size_t user_index) {
    ATTR_LIST_DUPLICATE_USER_INDEX_CHK(&config->local_state_list,
                                       struct local_state_attr, user_index);
    struct local_state_attr *attr;
    ATTR_LIST_ITEM_ALLOC(&config->local_state_list, user_index,
                         config->num_local_states,
                         ALGO_CONF_MAX_LOCAL_STATE_VARS, attr);
    return SUCCESS;
}

int algorithm_config_local_state_set(struct algorithm_config *config,
                                     size_t user_index, int initial_value) {
    struct local_state_attr *attr;
    ATTR_LIST_ITEM_SET(&config->local_state_list, struct local_state_attr,
                       user_index, initial_value, attr);
    return SUCCESS;
}