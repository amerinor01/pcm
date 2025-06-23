#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <stdbool.h>

#include "pcm.h"

typedef struct device device_t;
typedef struct flow flow_t;
typedef void *(*traffic_gen_fn_t)(void *);

typedef enum device_scheduler_progress_type {
    DEVICE_SCHEDULER_PROGRESS_AUTO = 0,
    DEVICE_SCHEDULER_PROGRESS_MANUAL = 1
} device_scheduler_progress_type_t;

typedef enum flow_status {
    FLOW_STATUS_STOP = 0,
    FLOW_STATUS_INIT = 1,
    FLOW_STATUS_RUNNING = 2
} flow_status_t;

int device_destroy(device_t *device);
int device_init(const char *device_name,
                device_scheduler_progress_type_t scheduler_progress,
                device_t **out);
bool device_scheduler_progress(device_t *device);

int flow_create(device_t *device, flow_t **flow,
                traffic_gen_fn_t traffic_gen_fn);
int flow_destroy(flow_t *flow);
int flow_time_init(flow_t *flow);
int flow_cwnd_get(const flow_t *flow);
void flow_signal_triggers_rearm(flow_t *flow);
void flow_signals_update(flow_t *flow, signal_t signal_type, int value);
void flow_status_set(flow_t *flow, flow_status_t new_status);
flow_status_t flow_status_get(const flow_t *flow);
void flow_error_set(flow_t *flow, int error);
int flow_error_get(const flow_t *flow);

#endif /* _NETWORK_H_ */