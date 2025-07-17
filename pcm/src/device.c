#include <unistd.h>

#include "htsim/htsim_flow.h"

#include "impl.h"
#include "pthread_flow.h"
#include "util.h"

int device_scheduler_init(struct scheduler *scheduler,
                          bool needs_progress_thread);
int device_scheduler_destroy(struct scheduler *scheduler);
static void *device_scheduler_thread_fn(void *arg);

int device_init(const char *flow_plugin_name, device_t **out) {

    device_t *device = calloc(1, sizeof(*device));
    if (!device) {
        LOG_CRIT("failed to allocate new device");
        return PCM_ERROR;
    }

    slist_init(&device->configs_list);

    bool needs_progress_thread = false;
    if (!strcmp(pthrd_flow_plugin_name, flow_plugin_name)) {
        if (pthrd_flow_ops_init(&device->flow_ops) != PCM_SUCCESS) {
            LOG_CRIT("failed to initialize flow backend %s", flow_plugin_name);
            goto destroy_scheduler;
        }
        needs_progress_thread = true;
    } else if (!strcmp(htsim_flow_plugin_name, flow_plugin_name)) {
        if (htsim_flow_ops_init(&device->flow_ops) != PCM_SUCCESS) {
            LOG_CRIT("failed to initialize flow backend %s", flow_plugin_name);
            goto destroy_scheduler;
        }
    } else {
        LOG_CRIT("unknown flow backend name %s", flow_plugin_name);
        goto destroy_scheduler;
    }

    if (device_scheduler_init(&device->scheduler, needs_progress_thread)) {
        LOG_CRIT("failed to initialize device scheduler");
        goto err;
    }

    *out = device;

    return PCM_SUCCESS;

destroy_scheduler:
    device_scheduler_destroy(&device->scheduler);
err:
    free(device);

    return PCM_ERROR;
}

int device_destroy(device_t *device) {
    int ret = PCM_SUCCESS;

    ret = device_scheduler_destroy(&device->scheduler);
    if (ret != PCM_SUCCESS)
        LOG_CRIT("[dev=%p] failed to destroy scheduler", device);

    if (!slist_empty(&device->configs_list)) {
        LOG_CRIT("[dev=%p] algorithm config list is not empty", device);
        while (!slist_empty(&device->configs_list)) { // avoid leak
            algorithm_config_destroy(
                container_of(slist_remove_head(&device->configs_list),
                             struct algorithm_config, list_entry));
        }
        ret = PCM_ERROR;
    }

    free(device);

    return ret;
}

int device_pcmc_init(device_t *dev_ctx, const char *algo_name,
                     pcm_handle_t *algo_handler) {
    pcm_handle_t new_handle;
    if (register_pcmc((void *)dev_ctx, 0, 0, 0, 0, &new_handle) != PCM_SUCCESS)
        return PCM_ERROR;

    if (register_algorithm_pcmc(algo_name, new_handle) != PCM_SUCCESS)
        return PCM_ERROR;

    if (activate_pcmc(new_handle) != PCM_SUCCESS)
        return PCM_ERROR;

    *algo_handler = new_handle;

    LOG_INFO("[dev=%p config=%p] algorithm %s registered and activated\n",
             dev_ctx, new_handle, algo_name);

    return PCM_SUCCESS;
}

int device_pcmc_destroy(pcm_handle_t algo_handler) {
    if (deactivate_pcmc(algo_handler) != PCM_SUCCESS)
        return PCM_ERROR;

    if (deregister_pcmc(algo_handler) != PCM_SUCCESS)
        return PCM_ERROR;

    return PCM_SUCCESS;
}

const struct algorithm_config *
device_flow_id_to_config_match(const device_t *device, pcm_addr_t addr) {
    struct slist_entry *item, *prev;
    slist_foreach(&device->configs_list, item, prev) {
        (void)prev; /* suppress complier warning */
        struct algorithm_config *config =
            container_of(item, struct algorithm_config, list_entry);
        /*
         * Rather than doing real matching, the code below mimics it with
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
                          bool needs_progress_thread) {
    slist_init(&scheduler->flow_list);

    scheduler->progress_auto = needs_progress_thread;

    if (scheduler->progress_auto) {
        scheduler->progress.thread.err = PCM_SUCCESS;
        if (pthread_mutex_init(&scheduler->progress.thread.flow_list_lock,
                               NULL))
            return PCM_ERROR;
        scheduler->progress.thread.running = true;
        if (pthread_create(&scheduler->progress.thread.pthread_obj, NULL,
                           device_scheduler_thread_fn, (void *)scheduler))
            goto err;
    } else {
        scheduler->progress.cur_flow = NULL;
    }

    return PCM_SUCCESS;

err:
    if (scheduler->progress_auto) {
        scheduler->progress.thread.err = PCM_ERROR;
        scheduler->progress.thread.running = false;
        pthread_mutex_destroy(&scheduler->progress.thread.flow_list_lock);
    }
    return PCM_ERROR;
}

int device_scheduler_destroy(struct scheduler *scheduler) {
    int ret = PCM_SUCCESS;

    if (scheduler->progress_auto) {
        if (!scheduler->progress.thread.running)
            return PCM_ERROR;
        scheduler->progress.thread.running = false;
        if (pthread_join(scheduler->progress.thread.pthread_obj, NULL))
            ret = PCM_ERROR;

        if (scheduler->progress.thread.err)
            ret = PCM_ERROR;

        if (pthread_mutex_destroy(&scheduler->progress.thread.flow_list_lock)) {
            ret = PCM_ERROR;
        }
    }

    return ret;
}

int device_scheduler_flow_add(struct scheduler *scheduler, flow_t *flow) {
    if (scheduler->progress_auto &&
        pthread_mutex_lock(&scheduler->progress.thread.flow_list_lock))
        return PCM_ERROR;

    slist_insert_tail(&flow->flow_list_entry, &scheduler->flow_list);

    if (scheduler->progress_auto &&
        pthread_mutex_unlock(&scheduler->progress.thread.flow_list_lock))
        return PCM_ERROR;

    return PCM_SUCCESS;
}

int device_scheduler_flow_remove(struct scheduler *scheduler, flow_t *flow) {
    if (scheduler->progress_auto &&
        pthread_mutex_lock(&scheduler->progress.thread.flow_list_lock))
        return PCM_ERROR;

    struct slist_entry *item, *prev;
    bool found = false;
    slist_foreach(&scheduler->flow_list, item, prev) {
        (void)prev; /* suppress complier warning */
        if (container_of(item, flow_t, flow_list_entry) == flow) {
            slist_remove(&scheduler->flow_list, item, prev);
            found = true;
            if (!scheduler->progress_auto)
                scheduler->progress.cur_flow = NULL;
            break;
        }
    }

    if (scheduler->progress_auto &&
        pthread_mutex_unlock(&scheduler->progress.thread.flow_list_lock))
        return PCM_ERROR;

    if (!found) {
        LOG_CRIT("[flow=%p addr=%u] flow was not found in the scheduler's "
                 "flow list",
                 flow, flow->addr);
        return PCM_ERROR;
    }

    return PCM_SUCCESS;
}

static void *device_scheduler_thread_fn(void *arg) {
    struct scheduler *scheduler = arg;

    LOG_DBG("scheduler thread started");

    size_t num_triggers = 0;
    (void)num_triggers; // make compiler happy in non-debug case

    while (scheduler->progress.thread.running) {
        if (pthread_mutex_lock(&scheduler->progress.thread.flow_list_lock)) {
            scheduler->progress.thread.err = PCM_ERROR;
            break;
        }

        struct slist_entry *item, *prev;
        slist_foreach(&scheduler->flow_list, item, prev) {
            (void)prev; /* suppress complier warning */
            flow_t *flow = container_of(item, flow_t, flow_list_entry);
            if (flow_handler_invoke_on_trigger(flow)) {
                num_triggers++;
            }
        }

        if (pthread_mutex_unlock(&scheduler->progress.thread.flow_list_lock))
            scheduler->progress.thread.err = PCM_ERROR;

        if (scheduler->progress.thread.err == PCM_ERROR)
            break;

        usleep(SCHEDULER_SLEEP_US);
    }

    LOG_DBG("scheduler thread finished, num_triggers=%zu, status=%d",
            num_triggers, scheduler->progress.thread.err);

    return NULL;
}

bool device_scheduler_progress(device_t *device) {
    if (slist_empty(&device->scheduler.flow_list))
        return false;

    if (device->scheduler.progress.cur_flow == NULL)
        device->scheduler.progress.cur_flow = device->scheduler.flow_list.head;

    flow_t *flow = container_of(device->scheduler.progress.cur_flow, flow_t,
                                flow_list_entry);

    bool triggered = false;
    if (flow_handler_invoke_on_trigger(flow))
        triggered = true;

    if (device->scheduler.progress.cur_flow == device->scheduler.flow_list.tail)
        device->scheduler.progress.cur_flow = device->scheduler.flow_list.head;
    else
        device->scheduler.progress.cur_flow =
            device->scheduler.progress.cur_flow->next;

    return triggered;
}