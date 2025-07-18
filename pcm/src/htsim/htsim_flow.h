#ifndef _HTSIM_FLOW_H_
#define _HTSIM_FLOW_H_

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "../impl.h"

#define HTSIM_MAX_REGFILE_SIZE (sizeof(uint64_t) * 64);

extern const char *htsim_flow_plugin_name;

struct htsim_flow {
    flow_t *flow;
    pcm_uint signals[ALGO_CONF_MAX_NUM_SIGNALS];
    pcm_uint thresholds[ALGO_CONF_MAX_NUM_SIGNALS];
    pcm_uint controls[ALGO_CONF_MAX_NUM_CONTROLS];
    pcm_uint vars[ALGO_CONF_MAX_VARS];
    uint64_t start_ts;
};

int htsim_flow_ops_init(struct flow_plugin_ops *flow_ops);

#endif /* _PTHREAD_FLOW_H_ */