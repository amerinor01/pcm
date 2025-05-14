#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pcc_sw_ctx.h"

#define container_of(ptr, type, member)                                        \
    ({                                                                         \
        const typeof(((type *)0)->member) *__mptr = (ptr);                     \
        (type *)((char *)__mptr - offsetof(type, member));                     \
    })

// Forward declaration
static void *pcc_flow_scheduler_thread_fn(void *arg);

int pcc_dev_ctx_init(const char *dev_name, struct pcc_dev_ctx **out) {
    (void)dev_name;
    struct pcc_dev_ctx *dev = calloc(1, sizeof(*dev));
    if (!dev)
        return PCC_ERROR;

    if (pthread_mutex_init(&dev->flow_list_lock, NULL))
        goto err;
    slist_init(&dev->flow_list);

    atomic_init(&dev->sched_running, true);
    if (pthread_create(&dev->sched_thread, NULL, pcc_flow_scheduler_thread_fn,
                       (void *)dev))
        goto err;

    *out = dev;

    return PCC_SUCCESS;

err:
    free(dev);

    return PCC_ERROR;
}

int pcc_dev_ctx_destroy(struct pcc_dev_ctx *dev) {
    int ret = PCC_SUCCESS;

    atomic_store(&dev->sched_running, false);
    if (pthread_join(dev->sched_thread, NULL))
        ret = PCC_ERROR;
    if (dev->sched_status)
        ret = dev->sched_status;

    if (pthread_mutex_destroy(&dev->flow_list_lock))
        ret = PCC_ERROR;

    free(dev);

    return ret;
}

int pcc_dev_ctx_caps_query(struct pcc_dev_ctx *dev,
                           struct pcc_algorithm_handler_attrs **caps_out) {
    (void)dev;
    /* For now we assume that backed supports everying */
    const uint32_t all_mask =
        PCC_SIG_TYPE_CLOCK | PCC_SIG_TYPE_CWND | PCC_SIG_TYPE_RATE |
        PCC_SIG_TYPE_PACER_DELAY | PCC_SIG_TYPE_RTT | PCC_SIG_TYPE_ACKED_BYTES |
        PCC_SIG_TYPE_ACKS_RECVD | PCC_SIG_TYPE_NACKS_RECVD |
        PCC_SIG_TYPE_ECNS_RECVD | PCC_SIG_TYPE_TRIM_RECVD | PCC_SIG_TYPE_RTOS;
    uint32_t all_signal_types[] = {
        PCC_SIG_TYPE_CLOCK,      PCC_SIG_TYPE_CWND,
        PCC_SIG_TYPE_RATE,       PCC_SIG_TYPE_PACER_DELAY,
        PCC_SIG_TYPE_RTT,        PCC_SIG_TYPE_ACKED_BYTES,
        PCC_SIG_TYPE_ACKS_RECVD, PCC_SIG_TYPE_NACKS_RECVD,
        PCC_SIG_TYPE_ECNS_RECVD, PCC_SIG_TYPE_TRIM_RECVD,
        PCC_SIG_TYPE_RTOS};
    size_t num_signals = sizeof(all_signal_types) / sizeof(all_signal_types[0]);

    struct pcc_signal_attr *sa = calloc(num_signals, sizeof(*sa));
    if (!sa)
        return PCC_ERROR;

    size_t i = 0;
    for (size_t j = 0; j < num_signals; j++) {
        if (all_mask & all_signal_types[j]) {
            sa[i].type = all_signal_types[j];
            sa[i].coalescing_op = PCC_SIG_COALESCING_OP_ACCUM;
            sa[i].reset_type = PCC_SIG_RESET_TYPE_USR;
            sa[i].dev_perms = PCC_SIG_READ | PCC_SIG_WRITE;
            sa[i].user_perms = PCC_SIG_READ | PCC_SIG_WRITE;
            sa[i].dtype = PCC_SIG_DTYPE_UINT64;
            sa[i].init_value = 0;
            i++;
        }
    }

    struct pcc_algorithm_handler_attrs *caps = calloc(1, sizeof(*caps));
    if (!caps) {
        free(sa);
        return PCC_ERROR;
    }

    caps->signals_mask = all_mask;
    caps->signal_attrs = sa;
    caps->num_signals = num_signals;
    caps->user_data_init = NULL;
    caps->user_data_size = 0;
    caps->cc_handler_fn = NULL;
    *caps_out = caps;

    return PCC_SUCCESS;
}

int pcc_dev_ctx_caps_free(struct pcc_algorithm_handler_attrs *caps) {
    if (!caps)
        return PCC_ERROR;
    free(caps->signal_attrs);
    free(caps);
    return PCC_SUCCESS;
}

int pcc_algorithm_handler_install(struct pcc_dev_ctx *dev,
                                  struct pcc_algorithm_handler_attrs *attrs,
                                  struct pcc_algorithm_handler_obj **out) {
    if (!dev)
        return PCC_ERROR;

    if (dev->handler)
        return PCC_ERROR;

    if (!attrs)
        return PCC_ERROR;

    /* TODO: validate attributes */

    struct pcc_algorithm_handler_obj *h = calloc(1, sizeof(*h));
    if (!h)
        return PCC_ERROR;

    /*
     * Save user attributes for later usage, e.g.,
     * datapath might use this info to setup signals
     */
    h->attrs = *attrs;
    h->attrs.signal_attrs =
        calloc(attrs->num_signals, sizeof(*h->attrs.signal_attrs));
    if (!h->attrs.signal_attrs) {
        free(h);
        return PCC_ERROR;
    }
    memcpy(h->attrs.signal_attrs, attrs->signal_attrs,
           attrs->num_signals * sizeof(*attrs->signal_attrs));

    dev->handler = h;
    *out = h;

    return PCC_SUCCESS;
}

int pcc_algorithm_handler_remove(struct pcc_algorithm_handler_obj *h) {
    free(h->attrs.signal_attrs);
    free(h);
    return PCC_SUCCESS;
}

int pcc_scheduler_flow_add(struct pcc_dev_ctx *dev, struct pcc_flow_ctx *flow) {
    if (pthread_mutex_lock(&dev->flow_list_lock))
        return PCC_ERROR;

    slist_insert_tail(&flow->flow_list_entry, &dev->flow_list);

    if (pthread_mutex_unlock(&dev->flow_list_lock))
        return PCC_ERROR;

    return PCC_SUCCESS;
}

int pcc_scheduler_flow_remove(struct pcc_dev_ctx *dev,
                              struct pcc_flow_ctx *flow) {
    if (pthread_mutex_lock(&dev->flow_list_lock))
        return PCC_ERROR;

    struct slist_entry *item, *prev;
    bool found = false;
    slist_foreach(&dev->flow_list, item, prev) {
        if (container_of(item, struct pcc_flow_ctx, flow_list_entry) == flow) {
            slist_remove(&dev->flow_list, item, prev);
            found = true;
        }
    }
    if (pthread_mutex_unlock(&dev->flow_list_lock))
        return PCC_ERROR;

    if (!found)
        return PCC_ERROR;

    return PCC_SUCCESS;
}

static void *pcc_flow_scheduler_thread_fn(void *arg) {
    struct pcc_dev_ctx *dev = arg;
    dev->sched_status = PCC_SUCCESS;

    while (atomic_load(&dev->sched_running)) {
        if (pthread_mutex_lock(&dev->flow_list_lock)) {
            dev->sched_status = PCC_ERROR;
            break;
        }

        struct slist_entry *item, *prev;
        slist_foreach(&dev->flow_list, item, prev) {
            dev->handler->attrs.cc_handler_fn(
                container_of(item, struct pcc_flow_ctx, flow_list_entry));
        }

        if (pthread_mutex_unlock(&dev->flow_list_lock)) {
            dev->sched_status = PCC_ERROR;
            break;
        }

        usleep(PCC_SCHEDULER_SLEEP_MS);
    }

    return NULL;
}