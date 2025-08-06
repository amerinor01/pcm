#ifndef _NETWORK_H_
#define _NETWORK_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdbool.h>

#include "pcmc.h"

typedef struct device *pcm_device_t;
typedef struct flow *pcm_flow_t;
typedef void *(*traffic_gen_fn_t)(void *);

pcm_err_t device_destroy(pcm_device_t device);
pcm_err_t device_init(const char *device_name, pcm_device_t *out);
pcm_err_t device_pcmc_init(pcm_device_t dev_ctx, const char *algo_name,
                           pcm_handle_t *algo_handler);
pcm_err_t device_pcmc_destroy(pcm_handle_t algo_handler);
bool device_scheduler_progress(pcm_device_t device, pcm_flow_t *triggered_flow);

pcm_err_t flow_create(pcm_device_t device, pcm_flow_t *flow,
                      traffic_gen_fn_t traffic_gen_fn);
pcm_err_t flow_destroy(pcm_flow_t flow);
bool flow_is_ready(const pcm_flow_t flow);
pcm_uint flow_cwnd_get(const pcm_flow_t flow);
void flow_signals_update(pcm_flow_t flow, pcm_signal_t signal_type,
                         pcm_uint value);
void flow_cwnd_set(const pcm_flow_t flow, pcm_uint cwnd);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _NETWORK_H_ */