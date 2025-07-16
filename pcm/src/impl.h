#ifndef _IMPL_H_
#define _IMPL_H_

#if defined(__cplusplus)
#include <atomic>
typedef std::atomic<bool> atomic_bool;
extern "C" {
#else
#include <stdatomic.h>
#endif

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "pcm.h"
#include "pcm_network.h"
#include "slist.h"

struct generic_metadata {
    struct slist_entry list_entry;
    size_t index;
    pcm_uint value;
};

// Forward declaration
struct signal_attr;

typedef void (*signal_accumulation_op_fn)(flow_t *, const struct signal_attr *,
                                          pcm_uint);
typedef bool (*signal_trigger_check_fn)(const flow_t *,
                                        const struct signal_attr *);
typedef void (*signal_trigger_arm_fn)(flow_t *, const struct signal_attr *);

extern signal_trigger_arm_fn flow_signal_trigger_arm_no_op;
extern signal_accumulation_op_fn flow_signal_accumulation_no_op;

struct signal_attr {
    struct generic_metadata metadata;
    pcm_signal_t type;
    pcm_signal_accum_t accum_type;
    signal_accumulation_op_fn accumulation_op_fn;
    bool is_trigger;
    signal_trigger_check_fn trigger_check_fn;
    signal_trigger_arm_fn trigger_arm_fn;
};

struct control_attr {
    struct generic_metadata metadata;
    pcm_control_t type;
};

struct local_state_attr {
    struct generic_metadata metadata;
};

struct constant_attr {
    struct generic_metadata metadata;
};

typedef int (*algo_function_t)(void *, void *, void *, void *, void *, void *);

#define ALGO_CONF_MAX_NUM_SIGNALS 16
#define ALGO_CONF_MAX_NUM_CONTROLS 2
#define ALGO_CONF_MAX_LOCAL_STATE_VARS 16
#define ALGO_CONF_MAX_NUM_CONSTANTS 16

struct algorithm_config {
    struct device *device;
    struct slist_entry list_entry;
    bool active;
    pcm_addr_mask_t matching_rule_mask;
    struct slist signals_list;
    size_t num_signals;
    struct slist controls_list;
    size_t num_controls;
    struct slist local_state_list;
    size_t num_local_states;
    struct slist constants_list;
    size_t num_constants;
    void *dlopen_handle;
    algo_function_t algorithm_fn;
};

struct flow_plugin_ops {
    struct control_ops {
        size_t (*max_regfile_size_get)();
        int (*create)(flow_t *, traffic_gen_fn_t);
        int (*destroy)(flow_t *);
        bool (*is_ready)(const flow_t *);
        pcm_uint (*time_get)(const flow_t *);
    } control;

    struct datapath_ops {
        signal_trigger_check_fn overflow_check;
        signal_trigger_check_fn timer_check;
        signal_trigger_check_fn burst_check;
        signal_trigger_arm_fn timer_reset;
        signal_trigger_arm_fn burst_reset;
        signal_accumulation_op_fn sum;
        signal_accumulation_op_fn last;
        signal_accumulation_op_fn min;
        signal_accumulation_op_fn max;
        signal_accumulation_op_fn elapsed_time;
    } datapath;

    struct handler_ops {
        void (*control_set)(void *, size_t, pcm_uint);
        pcm_uint (*control_get)(const void *, size_t);
        void (*signal_set)(void *, size_t, pcm_uint);
        pcm_uint (*signal_get)(const void *, size_t);
        void (*signal_update)(void *, size_t, pcm_uint);
        pcm_int (*local_state_int_get)(const void *, size_t);
        void (*local_state_int_set)(void *, size_t, pcm_int);
        pcm_uint (*local_state_uint_get)(const void *, size_t);
        void (*local_state_uint_set)(void *, size_t, pcm_uint);
        pcm_float (*local_state_float_get)(const void *, size_t);
        void (*local_state_float_set)(void *, size_t, pcm_float);
        pcm_int (*constant_int_get)(const void *, size_t);
        pcm_uint (*constant_uint_get)(const void *, size_t);
        pcm_float (*constant_float_get)(const void *, size_t);
    } handler;
};

struct flow {
    device_t *device;
    pcm_addr_t addr;
    struct slist_entry flow_list_entry;
    const struct algorithm_config *config;
    struct slist_entry *cur_trigger;
    size_t trigger_user_index;
    void *backend_ctx;
    // Note: this is a hacky way to get signals/ctrls/etc to get exposed
    // directly to the handler
    void *signals;
    void *thresholds;
    void *controls;
    void *local_state;
    void *constants;
};

#define SCHEDULER_SLEEP_US 1000 // 10 ms

struct scheduler {
    struct slist flow_list;
    bool progress_auto;
    union progress_impl {
        struct scheduler_progress_thread {
            pthread_mutex_t flow_list_lock;
            pthread_t pthread_obj;
            atomic_bool running;
            int err;
        } thread;
        struct slist_entry *cur_flow;
    } progress;
};

struct device {
    struct flow_plugin_ops flow_ops;
    pcm_addr_t flow_addr_counter;
    struct slist configs_list;
    struct scheduler scheduler;
};

int algorithm_config_alloc(device_t *device, struct algorithm_config **config);
int algorithm_config_destroy(struct algorithm_config *config);
int algorithm_config_matching_rule_add(struct algorithm_config *config,
                                       pcm_addr_mask_t matching_rule_mask);
int algorithm_config_activate(struct algorithm_config *config);
int algorithm_config_deactivate(struct algorithm_config *config);
int algorithm_config_signal_add(struct algorithm_config *config,
                                pcm_signal_t signal, pcm_signal_accum_t accum_type,
                                size_t user_index);
int algorithm_config_signal_trigger_set(struct algorithm_config *config,
                                        size_t user_index, pcm_uint threshold);
int algorithm_config_control_add(struct algorithm_config *config,
                                 pcm_control_t control, size_t user_index);
int algorithm_config_control_initial_value_set(struct algorithm_config *config,
                                               size_t user_index,
                                               pcm_uint initial_value);
int algorithm_config_local_state_add(struct algorithm_config *config,
                                     size_t user_index);
int algorithm_config_local_state_int_set(struct algorithm_config *config,
                                         size_t user_index,
                                         pcm_int initial_value);
int algorithm_config_local_state_uint_set(struct algorithm_config *config,
                                          size_t user_index,
                                          pcm_uint initial_value);
int algorithm_config_local_state_float_set(struct algorithm_config *config,
                                           size_t user_index,
                                           pcm_float initial_value);
int algorithm_config_constant_add(struct algorithm_config *config,
                                  size_t user_index);
int algorithm_config_constant_uint_set(struct algorithm_config *config,
                                       size_t user_index, pcm_uint value);
int algorithm_config_constant_int_set(struct algorithm_config *config,
                                      size_t user_index, pcm_int value);
int algorithm_config_constant_float_set(struct algorithm_config *config,
                                        size_t user_index, pcm_float value);
int algorithm_config_compile(struct algorithm_config *config,
                             const char *compile_path, char **err);
int device_scheduler_flow_add(struct scheduler *scheduler, flow_t *flow);
int device_scheduler_flow_remove(struct scheduler *scheduler, flow_t *flow);
const struct algorithm_config *
device_flow_id_to_config_match(const device_t *device, pcm_addr_t id);

void flow_triggers_arm(flow_t *flow);
bool flow_handler_invoke_on_trigger(flow_t *flow);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _IMPL_H_ */