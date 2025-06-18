#ifndef PCC_SW_CTX_H
#define PCC_SW_CTX_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>

#include "pcc.h"
#include "slist.h"

struct pcc_algorithm_handler_obj {
    struct pcc_algorithm_handler_attrs attrs;
};

struct pcc_flow_ctx {
    struct slist_entry flow_list_entry;
    void *signal_storage;
};

struct pcc_dev_ctx {
    struct pcc_algorithm_handler_obj *handler;
    pthread_mutex_t flow_list_lock;
    struct slist flow_list;
    pthread_t sched_thread;
    atomic_bool sched_running;
    int sched_status;
};

#define PCC_SCHEDULER_SLEEP_MS 10000 // 10 ms

int pcc_scheduler_flow_add(struct pcc_dev_ctx *dev, struct pcc_flow_ctx *flow);
int pcc_scheduler_flow_remove(struct pcc_dev_ctx *dev,
                              struct pcc_flow_ctx *flow);

#endif /* PCC_SW_CTX_H */