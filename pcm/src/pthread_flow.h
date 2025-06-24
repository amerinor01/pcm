#ifndef _PTHREAD_FLOW_H_
#define _PTHREAD_FLOW_H_

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "impl.h"

extern const char *pthrd_flow_plugin_name;

struct pthrd_flow {
    flow_t *flow;
    atomic_int signals[ALGO_CONF_MAX_NUM_SIGNALS];
    atomic_int thresholds[ALGO_CONF_MAX_NUM_SIGNALS];
    atomic_int controls[ALGO_CONF_MAX_NUM_CONTROLS];
    uint64_t local_state[ALGO_CONF_MAX_LOCAL_STATE_VARS];
    struct timespec start_ts;
    pthread_t pthread_obj;
    atomic_bool running;
};

int pthrd_flow_ops_init(struct flow_plugin_ops *flow_ops);

#endif /* _PTHREAD_FLOW_H_ */