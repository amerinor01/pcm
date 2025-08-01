#include <dlfcn.h>

#include "impl.h"
#include "util.h"

static int shared_symbol_open(const char *lib_name, char *fn_name,
                              void **so_handle, void **fn_ptr) {

    *so_handle = dlopen(lib_name, RTLD_NOW | RTLD_LOCAL);
    if (!(*so_handle)) {
        PCM_LOG_CRIT("dlopen(%s) failed with %s", lib_name, dlerror());
        return PCM_ERROR;
    }

    *fn_ptr = dlsym(*so_handle, fn_name);
    if (!(*fn_ptr)) {
        dlclose(*so_handle);
        PCM_LOG_CRIT("dlsym(%s) failed with %s", fn_name, dlerror());
        return PCM_ERROR;
    }

    PCM_LOG_DBG("dlsym(%s, %s)=%p", lib_name, fn_name, *fn_ptr);

    return PCM_SUCCESS;
}

static int shared_symbol_close(void *so_handle) {
    if (dlclose(so_handle)) {
        PCM_LOG_CRIT("dlclose() failed with %s", dlerror());
        return PCM_ERROR;
    }
    return PCM_SUCCESS;
}

int algorithm_config_alloc(pcm_device_t device,
                           struct algorithm_config **config) {
    if (!device)
        return PCM_ERROR;

    struct algorithm_config *new_config = calloc(1, sizeof(*new_config));
    if (!new_config) {
        PCM_LOG_CRIT("[dev=%p] failed to allocate new algorithm config",
                     device);
        return PCM_ERROR;
    }

    new_config->device = device;
    slist_insert_tail(&new_config->list_entry, &device->configs_list);
    slist_init(&new_config->signals_list);
    slist_init(&new_config->controls_list);
    slist_init(&new_config->var_list);
    new_config->active = false;

    PCM_LOG_DBG("[dev=%p] allocated new config=%p", device, new_config);
    *config = new_config;

    return PCM_SUCCESS;
}

int algorithm_config_destroy(struct algorithm_config *config) {
    int ret = PCM_SUCCESS;

    struct slist_entry *item, *prev;
    bool found = false;
    slist_foreach(&config->device->configs_list, item, prev) {
        if (container_of(item, struct algorithm_config, list_entry) == config) {
            slist_remove(&config->device->configs_list, item, prev);
            found = true;
        }
    }

    if (!found) {
        PCM_LOG_CRIT("[dev=%p conf=%p] config not found", config->device,
                     config);
        ret = PCM_ERROR;
    }

    ATTR_LIST_FREE(&config->signals_list, struct signal_attr,
                   config->num_signals);
    ATTR_LIST_FREE(&config->controls_list, struct control_attr,
                   config->num_controls);
    ATTR_LIST_FREE(&config->var_list, struct var_attr, config->num_vars);

    if (shared_symbol_close(config->dlopen_handle) != PCM_SUCCESS) {
        PCM_LOG_CRIT("[dev=%p conf=%p] failed to close handler dlopen object",
                     config->device, config);
        ret = PCM_ERROR;
    }

    PCM_LOG_DBG("[dev=%p] destroyed config=%p", config->device, config);
    free(config);

    return ret;
}

int algorithm_config_compile(struct algorithm_config *config,
                             const char *algo_name) {
    if ((strlen(algo_name) + 1) > PCMC_MAX_LEN_ALGO_NAME) {
        PCM_LOG_CRIT("Algorithm name length exceeds limit of %d characters",
                     PCMC_MAX_LEN_ALGO_NAME);
        return PCM_ERROR;
    }

    char lib_name[PCMC_MAX_LIB_NAME];

    // 1. finish the PCMC handle initialization
    // open lib<algo_name>_pcmc.so and call __<algo_name>_pcmc_init()
    if (sprintf(lib_name, "lib%s_pcmc.so", algo_name) < 0) {
        PCM_LOG_CRIT("[dev=%p conf=%p] failed to format PCMC init library name",
                     config->device, config);
        return PCM_ERROR;
    }

    char pcmc_init_fn_name[PCMC_MAX_INIT_FN_NAME];
    if (sprintf(pcmc_init_fn_name, "__%s_pcmc_init", algo_name) < 0) {
        PCM_LOG_CRIT(
            "[dev=%p conf=%p] failed to format PCMC init function name",
            config->device, config);
        return PCM_ERROR;
    }

    pcmc_init_function_t pcmc_init_fn = NULL;
    void *so_handle = NULL;
    void *raw_fn_ptr = NULL;
    if (shared_symbol_open(lib_name, pcmc_init_fn_name, &so_handle,
                           &raw_fn_ptr))
        return PCM_ERROR;
    pcmc_init_fn = (pcmc_init_function_t)raw_fn_ptr;

    if (pcmc_init_fn(config) != PCM_SUCCESS) {
        PCM_LOG_CRIT("[dev=%p conf=%p] %s failed to initialize PCMC config",
                     config->device, config, pcmc_init_fn_name);
        return PCM_ERROR;
    }

    if (shared_symbol_close(so_handle) != PCM_SUCCESS) {
        PCM_LOG_CRIT("[dev=%p conf=%p] failed to close %s object",
                     config->device, config, lib_name);
        return PCM_ERROR;
    }

    // 2. Cache pointer to the algorithm entry point in the config
    if (sprintf(lib_name, "lib%s.so", algo_name) < 0) {
        PCM_LOG_CRIT("[dev=%p conf=%p] failed to format algorithm library name",
                     config->device, config);
        return PCM_ERROR;
    }

    raw_fn_ptr = NULL;
    if (shared_symbol_open(lib_name, __algorithm_entry_point_symbol,
                           &config->dlopen_handle, &raw_fn_ptr))
        return PCM_ERROR;
    config->algorithm_fn = (algo_function_t)raw_fn_ptr;
    // config->dlopen_handle is closed upon the config gets distroyed

    return PCM_SUCCESS;
}

int algorithm_config_activate(struct algorithm_config *config) {
    if (config->active) {
        PCM_LOG_CRIT("[dev=%p conf=%p] config activation called twice",
                     config->device, config);
        return PCM_ERROR;
    }
    if (!config->algorithm_fn) {
        PCM_LOG_CRIT(
            "[dev=%p conf=%p] algorithm handler function must be compiled",
            config->device, config);
        return PCM_ERROR;
    }
    if (!config->num_signals) {
        PCM_LOG_CRIT(
            "[dev=%p conf=%p] at least one signal must be registered with "
            "configuration file",
            config->device, config);
        return PCM_ERROR;
    }
    if (!config->num_controls) {
        PCM_LOG_CRIT(
            "[dev=%p conf=%p] at least one control must be registered with "
            "configuration file",
            config->device, config);
        return PCM_ERROR;
    }
    config->active = true;
    PCM_LOG_DBG("[dev=%p conf=%p] activated, num_signals=%zu num_controls=%zu",
                config->device, config, config->num_signals,
                config->num_controls);
    return PCM_SUCCESS;
}

int algorithm_config_deactivate(struct algorithm_config *config) {
    if (!config->active) {
        PCM_LOG_CRIT("[dev=%p conf=%p] config deactivation called twice",
                     config->device, config);
        return PCM_ERROR;
    }
    config->active = false;
    PCM_LOG_DBG("[dev=%p conf=%p] deactivated", config->device, config);
    return PCM_SUCCESS;
}

int algorithm_config_matching_rule_add(struct algorithm_config *config,
                                       pcm_addr_mask_t matching_rule_mask) {
    config->matching_rule_mask = matching_rule_mask;
    PCM_LOG_DBG("[dev=%p conf=%p] added matching rule=0x%x", config->device,
                config, matching_rule_mask);
    return PCM_SUCCESS;
}

int algorithm_config_signal_add(struct algorithm_config *config,
                                pcm_signal_t signal,
                                pcm_signal_accum_t accum_type, size_t idx) {
    ATTR_LIST_DUPLICATE_IDX_CHK(&config->signals_list, struct signal_attr, idx);
    struct signal_attr *attr;
    ATTR_LIST_ITEM_ALLOC(&config->signals_list, idx, config->num_signals,
                         ALGO_CONF_MAX_NUM_SIGNALS, attr, true);

    attr->type = signal;
    attr->accum_type = accum_type;
    switch (attr->accum_type) {
    case PCM_SIG_ACCUM_SUM:
        attr->accumulation_op_fn = config->device->flow_ops.datapath.sum;
        break;
    case PCM_SIG_ACCUM_LAST:
        attr->accumulation_op_fn = config->device->flow_ops.datapath.last;
        break;
    case PCM_SIG_ACCUM_MIN:
        attr->accumulation_op_fn = config->device->flow_ops.datapath.min;
        break;
    case PCM_SIG_ACCUM_MAX:
        attr->accumulation_op_fn = config->device->flow_ops.datapath.max;
        break;
    default:
        PCM_LOG_CRIT(
            "[dev=%p conf=%p] unknown or unsupported signal accumulation "
            "type requested",
            config->device, config);
        return PCM_ERROR;
    }

    if (attr->type == PCM_SIG_ELAPSED_TIME)
        attr->accumulation_op_fn =
            config->device->flow_ops.datapath.elapsed_time;

    return PCM_SUCCESS;
}

int algorithm_config_signal_trigger_set(struct algorithm_config *config,
                                        size_t idx, pcm_uint threshold) {
    struct signal_attr *attr;
    ATTR_LIST_ITEM_SET(&config->signals_list, struct signal_attr, idx,
                       threshold, attr);
    if (threshold <= 0)
        return PCM_ERROR;

    attr->trigger_check_fn = config->device->flow_ops.datapath.overflow_check;
    attr->trigger_arm_fn = flow_signal_trigger_arm_no_op;

    // trigger on elapsed time (timer) relies on aux state
    if (attr->type == PCM_SIG_ELAPSED_TIME) {
        attr->accumulation_op_fn = flow_signal_accumulation_no_op;
        attr->trigger_check_fn = config->device->flow_ops.datapath.timer_check;
        attr->trigger_arm_fn = config->device->flow_ops.datapath.timer_reset;
    } else if (attr->type == PCM_SIG_DATA_TX) {
        attr->trigger_check_fn = config->device->flow_ops.datapath.burst_check;
        attr->trigger_arm_fn = config->device->flow_ops.datapath.burst_reset;
    }

    attr->is_trigger = true;
    return PCM_SUCCESS;
}

int algorithm_config_control_add(struct algorithm_config *config,
                                 pcm_control_t control, size_t idx) {
    ATTR_LIST_DUPLICATE_IDX_CHK(&config->controls_list, struct control_attr,
                                idx);
    ATTR_LIST_DUPLICATE_TYPE_CHK(&config->controls_list, struct control_attr,
                                 control);
    struct control_attr *attr;
    ATTR_LIST_ITEM_ALLOC(&config->controls_list, idx, config->num_controls,
                         ALGO_CONF_MAX_NUM_CONTROLS, attr, false);
    attr->type = control;
    return PCM_SUCCESS;
}

int algorithm_config_control_initial_value_set(struct algorithm_config *config,
                                               size_t idx,
                                               pcm_uint initial_value) {
    struct control_attr *attr;
    ATTR_LIST_ITEM_SET(&config->controls_list, struct control_attr, idx,
                       initial_value, attr);
    return PCM_SUCCESS;
}

int algorithm_config_var_add(struct algorithm_config *config, size_t idx) {
    ATTR_LIST_DUPLICATE_IDX_CHK(&config->var_list, struct var_attr, idx);
    struct var_attr *attr;
    ATTR_LIST_ITEM_ALLOC(&config->var_list, idx, config->num_vars,
                         ALGO_CONF_MAX_VARS, attr, false);
    return PCM_SUCCESS;
}

int algorithm_config_var_uint_set(struct algorithm_config *config, size_t idx,
                                  pcm_uint initial_value) {
    struct var_attr *attr;
    ATTR_LIST_ITEM_SET(&config->var_list, struct var_attr, idx, initial_value,
                       attr);
    return PCM_SUCCESS;
}

int algorithm_config_var_int_set(struct algorithm_config *config, size_t idx,
                                 pcm_int initial_value) {
    struct var_attr *attr;
    pcm_uint encoded_val = encode_pcm_int(initial_value);
    ATTR_LIST_ITEM_SET(&config->var_list, struct var_attr, idx, encoded_val,
                       attr);
    return PCM_SUCCESS;
}

int algorithm_config_var_float_set(struct algorithm_config *config, size_t idx,
                                   pcm_float initial_value) {
    struct var_attr *attr;
    pcm_uint encoded_val = encode_pcm_float(initial_value);
    ATTR_LIST_ITEM_SET(&config->var_list, struct var_attr, idx, encoded_val,
                       attr);
    return PCM_SUCCESS;
}