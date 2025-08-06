#ifndef _PTHREAD_FLOW_H_
#define _PTHREAD_FLOW_H_

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "impl.h"

#define PTHRD_MAX_REGFILE_SIZE (sizeof(uint64_t) * 64);

extern const char *pthrd_flow_plugin_name;

struct pthrd_flow {
    pcm_flow_t flow;
    atomic_uint_fast64_t signals[ALGO_CONF_MAX_NUM_SIGNALS];
    atomic_uint_fast64_t thresholds[ALGO_CONF_MAX_NUM_SIGNALS];
    atomic_uint_fast64_t
        controls[ALGO_CONF_MAX_NUM_CONTROLS]; // can controls be non-atomic?
    struct timespec start_ts;
    pthread_t pthread_obj;
    atomic_bool running;
};

pcm_err_t pthrd_flow_ops_init(struct flow_plugin_ops *flow_ops);

#endif /* _PTHREAD_FLOW_H_ */