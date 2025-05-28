#ifndef _NETWORK_H_
#define _NETWORK_H_

typedef struct device device_t;
typedef struct flow flow_t;
typedef void *(*traffic_gen_fn_t)(void *);

int device_destroy(device_t *device);
int device_init(const char *device_name, device_t **out);
int flow_create(device_t *device, flow_t **flow,
                traffic_gen_fn_t traffic_gen_fn);
int flow_destroy(flow_t *flow);

#endif /* _NETWORK_H_ */