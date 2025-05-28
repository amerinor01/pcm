#ifndef _DRIVER_H_
#define _DRIVER_H_

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include "network.h"
#include "pcm.h"
#include "slist.h"
#include "lwlog.h"

#define LOG_DBG(FORMAT, ...) \
{ \
    lwlog_debug(FORMAT, ##__VA_ARGS__); \
}

#define LOG_CRIT(FORMAT, ...) \
{ \
    lwlog_crit(FORMAT, ##__VA_ARGS__); \
}

#define LOG_PRINT(FORMAT, ...)                                                                   \
    {                                                                                              \
        lwlog_info(FORMAT, ##__VA_ARGS__);                                                        \
    }

struct generic_metadata {
    struct slist_entry list_entry;
    size_t index;
    int value;
};

struct signal_attr {
    struct generic_metadata metadata;
    signal_t type;
    signal_accum_t accumulate_op;
    bool is_trigger;
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
    int matching_rule_mask;
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
#define TGEN_PACKET_SIZE 1500          // bytes per packet (MSS)
#define TGEN_THREAD_SLEEP_TIME_US 1000

struct flow {
    uint32_t flow_id;
    struct slist_entry flow_list_entry;
    const struct algorithm_config *config;
    atomic_int datapath_state[FLOW_DATAPATH_STATE_SIZE];
    int local_state[FLOW_LOCAL_STATE_SIZE];
    pthread_t thread;
    atomic_bool running;
    int status;
};

#define SCHEDULER_SLEEP_US 10000 // 10 ms

struct scheduler {
    pthread_mutex_t flow_list_lock;
    struct slist flow_list;
    pthread_t thread;
    atomic_bool running;
    int status;
};

struct device {
    uint32_t flow_id_counter;
    struct slist configs_list;
    struct scheduler scheduler;
};

int algorithm_config_alloc(device_t *device, struct algorithm_config **config);
int algorithm_config_destroy(struct algorithm_config *config);
int algorithm_config_matching_rule_add(struct algorithm_config *config,
                                       int matching_rule_mask);
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

#endif /* _DRIVER_H_ */