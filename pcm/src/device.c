#include <unistd.h>

#include "impl.h"
#include "util.h"

int device_scheduler_init(struct scheduler *scheduler,
                          device_scheduler_progress_type_t sched_progress);
int device_scheduler_destroy(struct scheduler *scheduler);
static void *device_scheduler_thread_fn(void *arg);

int device_init(const char *device_name,
                device_scheduler_progress_type_t sched_progress,
                device_t **out) {
    (void)device_name;
    device_t *device = calloc(1, sizeof(*device));
    if (!device) {
        LOG_CRIT("failed to allocate new device");
        return ERROR;
    }

    slist_init(&device->configs_list);

    if (device_scheduler_init(&device->scheduler, sched_progress)) {
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
device_flow_id_to_config_match(const device_t *device, addr_t addr) {
    struct slist_entry *item, *prev;
    slist_foreach(&device->configs_list, item, prev) {
        (void)prev; /* suppress complier warning */
        struct algorithm_config *config =
            container_of(item, struct algorithm_config, list_entry);
        /*
         * Rather than doing real matching below, the code below mimics it with
         * logical OR.
         */
        if (config->active && (config->matching_rule_mask | addr)) {
            LOG_DBG("[dev=%p] matched flow addr=%u to config=%p", device, addr,
                    config);
            return config;
        }
    }
    return NULL;
}

int device_scheduler_init(struct scheduler *scheduler,
                          device_scheduler_progress_type_t sched_progress) {
    slist_init(&scheduler->flow_list);

    scheduler->progress_auto =
        sched_progress == DEVICE_SCHEDULER_PROGRESS_AUTO ? true : false;

    if (scheduler->progress_auto) {
        scheduler->status = SUCCESS;
        if (pthread_mutex_init(&scheduler->flow_list_lock, NULL))
            return ERROR;
        scheduler->running = true;
        if (pthread_create(&scheduler->thread, NULL, device_scheduler_thread_fn,
                           (void *)scheduler))
            goto err;
    } else {
        scheduler->cur_flow = NULL;
    }

    return SUCCESS;

err:
    if (scheduler->progress_auto) {
        scheduler->status = ERROR;
        scheduler->running = false;
        pthread_mutex_destroy(&scheduler->flow_list_lock);
    }
    return ERROR;
}

int device_scheduler_destroy(struct scheduler *scheduler) {
    int ret = SUCCESS;

    if (scheduler->progress_auto) {
        if (!scheduler->running)
            return ERROR;
        scheduler->running = false;
        if (pthread_join(scheduler->thread, NULL))
            ret = ERROR;

        if (scheduler->status)
            ret = ERROR;

        if (pthread_mutex_destroy(&scheduler->flow_list_lock))
            ret = ERROR;
    }

    return ret;
}

int device_scheduler_flow_add(struct scheduler *scheduler, flow_t *flow) {
    if (scheduler->progress_auto &&
        pthread_mutex_lock(&scheduler->flow_list_lock))
        return ERROR;

    slist_insert_tail(&flow->flow_list_entry, &scheduler->flow_list);

    if (scheduler->progress_auto &&
        pthread_mutex_unlock(&scheduler->flow_list_lock))
        return ERROR;

    return SUCCESS;
}

int device_scheduler_flow_remove(struct scheduler *scheduler, flow_t *flow) {
    if (scheduler->progress_auto &&
        pthread_mutex_lock(&scheduler->flow_list_lock))
        return ERROR;

    struct slist_entry *item, *prev;
    bool found = false;
    slist_foreach(&scheduler->flow_list, item, prev) {
        (void)prev; /* suppress complier warning */
        if (container_of(item, flow_t, flow_list_entry) == flow) {
            slist_remove(&scheduler->flow_list, item, prev);
            found = true;
            break;
        }
    }

    if (scheduler->progress_auto &&
        pthread_mutex_unlock(&scheduler->flow_list_lock))
        return ERROR;

    if (!found) {
        LOG_CRIT("[flow=%p addr=%u] flow was not found in the scheduler's "
                 "flow list",
                 flow, flow->addr);
        return ERROR;
    }

    return SUCCESS;
}

static void *device_scheduler_thread_fn(void *arg) {
    struct scheduler *scheduler = arg;

    LOG_DBG("scheduler thread started");

    size_t num_triggers = 0;
    while (scheduler->running) {
        if (pthread_mutex_lock(&scheduler->flow_list_lock)) {
            scheduler->status = ERROR;
            break;
        }

        struct slist_entry *item, *prev;
        slist_foreach(&scheduler->flow_list, item, prev) {
            (void)prev; /* suppress complier warning */
            flow_t *flow = container_of(item, flow_t, flow_list_entry);
            if (flow_state_get(flow) != FLOW_STATE_RUNNING) {
                scheduler->status = ERROR;
                break;
            }
            if (flow_handler_invoke_on_trigger(flow)) {
                num_triggers++;
            }
        }

        if (pthread_mutex_unlock(&scheduler->flow_list_lock))
            scheduler->status = ERROR;

        if (scheduler->status == ERROR)
            break;

        usleep(SCHEDULER_SLEEP_US);
    }

    LOG_DBG("scheduler thread finished, num_triggers=%zu, status=%d",
            num_triggers, scheduler->status);

    return NULL;
}

bool device_scheduler_progress(device_t *device) {
    if (slist_empty(&device->scheduler.flow_list))
        return false;

    if (device->scheduler.cur_flow == NULL)
        device->scheduler.cur_flow = device->scheduler.flow_list.head;

    flow_t *flow =
        container_of(device->scheduler.cur_flow, flow_t, flow_list_entry);

    bool triggered = false;
    if ((flow_state_get(flow) == FLOW_STATE_RUNNING) &&
        flow_handler_invoke_on_trigger(flow))
        triggered = true;

    if (device->scheduler.cur_flow == device->scheduler.flow_list.tail)
        device->scheduler.cur_flow = device->scheduler.flow_list.head;

    return triggered;
}