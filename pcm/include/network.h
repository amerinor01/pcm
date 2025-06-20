#ifndef _NETWORK_H_
#define _NETWORK_H_

#include "pcm.h"

typedef struct device device_t;
typedef struct flow flow_t;
typedef void *(*traffic_gen_fn_t)(void *);

typedef enum flow_state {
    FLOW_THREAD_STOP = 0,
    FLOW_THREAD_INIT = 1,
    FLOW_THREAD_RUNNING = 2
} flow_state_t;

int flow_time_init(flow_t *flow);
int flow_cwnd_get(const flow_t *flow);
void flow_signal_triggers_rearm(flow_t *flow);
void flow_signals_update(flow_t *flow, signal_t signal_type, int value);
void flow_progress_state_set(flow_t *flow, flow_state_t new_state);
flow_state_t flow_progress_state_get(const flow_t *flow);
void flow_error_status_set(flow_t *flow, int status);
int flow_error_status_get(const flow_t *flow);

int device_destroy(device_t *device);
int device_init(const char *device_name, device_t **out);
int flow_create(device_t *device, flow_t **flow,
                traffic_gen_fn_t traffic_gen_fn);
int flow_destroy(flow_t *flow);

#endif /* _NETWORK_H_ */