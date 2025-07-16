#ifndef _NETWORK_H_
#define _NETWORK_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdbool.h>

#include "pcm.h"

typedef struct device device_t;
typedef struct flow flow_t;
typedef void *(*traffic_gen_fn_t)(void *);

int device_destroy(device_t *device);
int device_init(const char *device_name, device_t **out);
bool device_scheduler_progress(device_t *device);

int flow_create(device_t *device, flow_t **flow,
                traffic_gen_fn_t traffic_gen_fn);
int flow_destroy(flow_t *flow);
bool flow_is_ready(const flow_t *flow);
pcm_uint flow_cwnd_get(const flow_t *flow);
void flow_signals_update(flow_t *flow, pcm_signal_t signal_type, pcm_uint value);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _NETWORK_H_ */