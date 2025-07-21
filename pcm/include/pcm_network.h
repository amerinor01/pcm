#ifndef _NETWORK_H_
#define _NETWORK_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdbool.h>

#include "pcm.h"

typedef struct device *pcm_device_t;
typedef struct flow *pcm_flow_t;
typedef void *(*traffic_gen_fn_t)(void *);

int device_destroy(pcm_device_t device);
int device_init(const char *device_name, pcm_device_t *out);
int device_pcmc_init(pcm_device_t dev_ctx, const char *algo_name,
                     pcm_handle_t *algo_handler);
int device_pcmc_destroy(pcm_handle_t algo_handler);
bool device_scheduler_progress(pcm_device_t device, pcm_flow_t *triggered_flow);

int flow_create(pcm_device_t device, pcm_flow_t *flow,
                traffic_gen_fn_t traffic_gen_fn);
int flow_destroy(pcm_flow_t flow);
bool flow_is_ready(const pcm_flow_t flow);
pcm_uint flow_cwnd_get(const pcm_flow_t flow);
void flow_signals_update(pcm_flow_t flow, pcm_signal_t signal_type,
                         pcm_uint value);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _NETWORK_H_ */