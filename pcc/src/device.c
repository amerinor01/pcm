#include <unistd.h>

#include "impl.h"
#include "util.h"

int device_scheduler_init(struct scheduler *scheduler);
int device_scheduler_destroy(struct scheduler *scheduler);
static void *device_scheduler_thread_fn(void *arg);

int device_init(const char *device_name, device_t **out) {
    (void)device_name;
    device_t *device = calloc(1, sizeof(*device));
    if (!device) {
        LOG_CRIT("failed to allocate new device");
        return ERROR;
    }

    slist_init(&device->configs_list);

    if (device_scheduler_init(&device->scheduler)) {
        LOG_CRIT("failed to initialize device scheduler");
        goto err;
    }

    *out = device;

    return SUCCESS;

err:
    free(device);

    return ERROR;
}

int device_destroy(device_t *device) {
    int ret = SUCCESS;

    ret = device_scheduler_destroy(&device->scheduler);
    if (ret != SUCCESS)
        LOG_CRIT("[dev=%p] failed to destroy scheduler", device);

    if (!slist_empty(&device->configs_list)) {
        LOG_CRIT("[dev=%p] algorithm config list is not empty", device);
        while (!slist_empty(&device->configs_list)) { // avoid leak
            algorithm_config_destroy(
                container_of(slist_remove_head(&device->configs_list),
                             struct algorithm_config, list_entry));
        }
        ret = ERROR;
    }

    free(device);

    return ret;
}

const struct algorithm_config *
device_flow_id_to_config_match(const device_t *device, uint32_t id) {
    struct slist_entry *item, *prev;
    slist_foreach(&device->configs_list, item, prev) {
        struct algorithm_config *config =
            container_of(item, struct algorithm_config, list_entry);
        /*
         * Rather than doing real matching below, the code below mimics it with
         * logical OR.
         */
        if (config->active && (config->matching_rule_mask | id)) {
            LOG_DBG("[dev=%p] matched flow id=%u to config=%p", device, id,
                    config);
            return config;
        }
    }
    return NULL;
}

int device_scheduler_init(struct scheduler *scheduler) {
    if (pthread_mutex_init(&scheduler->flow_list_lock, NULL))
        return ERROR;

    slist_init(&scheduler->flow_list);

    scheduler->status = SUCCESS;
    atomic_init(&scheduler->running, true);
    if (pthread_create(&scheduler->thread, NULL, device_scheduler_thread_fn,
                       (void *)scheduler))
        goto err;

    return SUCCESS;

err:
    scheduler->status = ERROR;
    atomic_init(&scheduler->running, false);
    pthread_mutex_destroy(&scheduler->flow_list_lock);
    return ERROR;
}

int device_scheduler_destroy(struct scheduler *scheduler) {
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

int device_scheduler_flow_add(struct scheduler *scheduler, flow_t *flow) {
    if (pthread_mutex_lock(&scheduler->flow_list_lock))
        return ERROR;

    slist_insert_tail(&flow->flow_list_entry, &scheduler->flow_list);

    if (pthread_mutex_unlock(&scheduler->flow_list_lock))
        return ERROR;

    return SUCCESS;
}

int device_scheduler_flow_remove(struct scheduler *scheduler, flow_t *flow) {
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

    if (!found) {
        LOG_CRIT("[flow=%p id=%u] flow was not found in the scheduler's "
                 "flow list",
                 flow, flow->id);
        return ERROR;
    }

    return SUCCESS;
}

static void *device_scheduler_thread_fn(void *arg) {
    struct scheduler *scheduler = arg;

    uint64_t tid;
    if (pthread_threadid_np(NULL, &tid)) {
        scheduler->status = ERROR;
        goto thread_termination;
    }

    LOG_DBG("[tid=%llu] scheduler started", tid);

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

thread_termination:
    LOG_DBG("[tid=%llu] scheduler finished with status=%d", tid,
            scheduler->status);

    return NULL;
}