#ifndef _IMPL_H_
#define _IMPL_H_

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include "network.h"
#include "pcm.h"
#include "slist.h"

struct generic_metadata {
    struct slist_entry list_entry;
    size_t index;
    int value;
};

// Forward declaration
struct signal_attr;

typedef void (*signal_accumulation_op_fn)(flow_t *, const struct signal_attr *,
                                          int);
typedef bool (*signal_trigger_check_fn)(const flow_t *,
                                        const struct signal_attr *);

struct signal_attr {
    struct generic_metadata metadata;
    signal_t type;
    signal_accum_t accum_type;
    signal_accumulation_op_fn accumulation_op_fn;
    bool is_trigger;
    signal_trigger_check_fn trigger_check_fn;
};

struct control_attr {
    struct generic_metadata metadata;
    control_t type;
};

struct local_state_attr {
    struct generic_metadata metadata;
};

typedef int (*algo_function_t)(void *);

#define ALGO_CONF_MAX_NUM_SIGNALS 16
#define ALGO_CONF_MAX_NUM_CONTROLS 2
#define ALGO_CONF_MAX_LOCAL_STATE_VARS 16

struct algorithm_config {
    struct device *device;
    struct slist_entry list_entry;
    bool active;
    addr_mask_t matching_rule_mask;
    struct slist signals_list;
    size_t num_signals;
    struct slist controls_list;
    size_t num_controls;
    struct slist local_state_list;
    size_t num_local_states;
    void *dlopen_handle;
    algo_function_t algorithm_fn;
};

#define FLOW_SIGNALS_OFFSET 0
#define FLOW_SIGNALS_THRESHOLDS_OFFSET ALGO_CONF_MAX_NUM_SIGNALS
#define FLOW_CONTROLS_OFFSET                                                   \
    (FLOW_SIGNALS_THRESHOLDS_OFFSET + ALGO_CONF_MAX_NUM_SIGNALS)
#define FLOW_DATAPATH_STATE_SIZE                                               \
    (2 * ALGO_CONF_MAX_NUM_SIGNALS + ALGO_CONF_MAX_NUM_CONTROLS)
#define FLOW_LOCAL_STATE_VARS_OFFSET 0
#define FLOW_LOCAL_STATE_SIZE ALGO_CONF_MAX_LOCAL_STATE_VARS

#define TGEN_BANDWIDTH_BPS 100000000UL // 100 Mbps
#define TGEN_DROP_PROB 0.01            // 1% packet drop probability
#define TGEN_NACK_PROB 0.02            // 2% NACK probability (duplicate ACK)
#define TGEN_ECN_CONG_PROB 0.3
#define TGEN_PACKET_SIZE 1500          // bytes per packet (MSS)
#define TGEN_THREAD_SLEEP_TIME_US 1000

struct flow {
    addr_t addr;
    struct slist_entry flow_list_entry;
    const struct algorithm_config *config;
    atomic_int datapath_state[FLOW_DATAPATH_STATE_SIZE];
    int local_state[FLOW_LOCAL_STATE_SIZE];
    pthread_t thread;
    atomic_bool running;
    int status;
    uint64_t tid;
};

#define SCHEDULER_SLEEP_US 1000 // 10 ms

struct scheduler {
    pthread_mutex_t flow_list_lock;
    struct slist flow_list;
    pthread_t thread;
    atomic_bool running;
    int status;
};

struct device {
    addr_t flow_addr_counter;
    struct slist configs_list;
    struct scheduler scheduler;
};

int algorithm_config_alloc(device_t *device, struct algorithm_config **config);
int algorithm_config_destroy(struct algorithm_config *config);
int algorithm_config_matching_rule_add(struct algorithm_config *config,
                                       addr_mask_t matching_rule_mask);
int algorithm_config_activate(struct algorithm_config *config);
int algorithm_config_deactivate(struct algorithm_config *config);
int algorithm_config_signal_add(struct algorithm_config *config,
                                signal_t signal, signal_accum_t accum_type,
                                size_t user_index);
int algorithm_config_signal_trigger_set(struct algorithm_config *config,
                                        size_t user_index, int threshold);
int algorithm_config_control_add(struct algorithm_config *config,
                                 control_t control, size_t user_index);
int algorithm_config_control_initial_value_set(struct algorithm_config *config,
                                               size_t user_index,
                                               int initial_value);
int algorithm_config_local_state_add(struct algorithm_config *config,
                                     size_t user_index);
int algorithm_config_local_state_set(struct algorithm_config *config,
                                     size_t user_index, int initial_value);
int algorithm_config_compile(struct algorithm_config *config,
                             const char *compile_path, char **err);
int device_scheduler_flow_add(struct scheduler *scheduler, flow_t *flow);
int device_scheduler_flow_remove(struct scheduler *scheduler, flow_t *flow);
const struct algorithm_config *
device_flow_id_to_config_match(const device_t *device, addr_t id);
bool flow_handler_trigger_check(const flow_t *flow,
                                const struct signal_attr *attr);
bool flow_triggers_check(const flow_t *flow);
void flow_signal_accumulation_op_sum(flow_t *flow,
                                     const struct signal_attr *attr,
                                     int signal);
void flow_signal_accumulation_op_last(flow_t *flow,
                                      const struct signal_attr *attr,
                                      int signal);
void flow_signal_accumulation_op_min(flow_t *flow,
                                     const struct signal_attr *attr,
                                     int signal);
void flow_signal_accumulation_op_max(flow_t *flow,
                                     const struct signal_attr *attr,
                                     int signal);

#endif /* _IMPL_H_ */