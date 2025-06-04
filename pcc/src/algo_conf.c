#include <dlfcn.h>

#include "impl.h"
#include "util.h"

int algorithm_config_alloc(device_t *device, struct algorithm_config **config) {
    if (!device)
        return ERROR;

    struct algorithm_config *new_config = calloc(1, sizeof(*new_config));
    if (!new_config) {
        LOG_CRIT("[dev=%p] failed to allocate new algorithm config", device);
        return ERROR;
    }

    new_config->device = device;
    slist_insert_tail(&new_config->list_entry, &device->configs_list);
    slist_init(&new_config->signals_list);
    slist_init(&new_config->controls_list);
    slist_init(&new_config->local_state_list);
    new_config->active = false;

    LOG_DBG("[dev=%p] allocated new config=%p", device, new_config);
    *config = new_config;

    return SUCCESS;
}

int algorithm_config_destroy(struct algorithm_config *config) {
    int ret = SUCCESS;

    struct slist_entry *item, *prev;
    bool found = false;
    slist_foreach(&config->device->configs_list, item, prev) {
        if (container_of(item, struct algorithm_config, list_entry) == config) {
            slist_remove(&config->device->configs_list, item, prev);
            found = true;
        }
    }

    if (!found) {
        LOG_CRIT("[dev=%p conf=%p] config not found", config->device, config);
        ret = ERROR;
    }

    ATTR_LIST_FREE(&config->signals_list, struct signal_attr,
                   config->num_signals);
    ATTR_LIST_FREE(&config->controls_list, struct control_attr,
                   config->num_controls);
    ATTR_LIST_FREE(&config->local_state_list, struct local_state_attr,
                   config->num_local_states);

    if (dlclose(config->dlopen_handle)) {
        LOG_CRIT("[dev=%p conf=%p] dlclose() failed with %s", config->device,
                 config, dlerror());
        ret = ERROR;
    }

    LOG_DBG("[dev=%p] destroyed config=%p", config->device, config);
    free(config);

    return ret;
}

int algorithm_config_activate(struct algorithm_config *config) {
    if (config->active)
        return ERROR;
    config->active = true;
    LOG_DBG("[dev=%p conf=%p] activated", config->device, config);
    return SUCCESS;
}

int algorithm_config_deactivate(struct algorithm_config *config) {
    if (!config->active)
        return ERROR;
    config->active = false;
    LOG_DBG("[dev=%p conf=%p] deactivated", config->device, config);
    return SUCCESS;
}

int algorithm_config_matching_rule_add(struct algorithm_config *config,
                                       int matching_rule_mask) {
    config->matching_rule_mask = matching_rule_mask;
    LOG_DBG("[dev=%p conf=%p] added matching rule=%d", config->device, config,
            matching_rule_mask);
    return SUCCESS;
}

int algorithm_config_signal_add(struct algorithm_config *config,
                                signal_t signal, signal_accum_t accum_type,
                                size_t user_index) {
    ATTR_LIST_DUPLICATE_USER_INDEX_CHK(&config->signals_list,
                                       struct signal_attr, user_index);
    struct signal_attr *attr;
    ATTR_LIST_ITEM_ALLOC(&config->signals_list, user_index, config->num_signals,
                         ALGO_CONF_MAX_NUM_SIGNALS, attr);

    attr->type = signal;

    switch (accum_type) {
    case SIG_ACCUM_SUM:
        attr->accumulation_op_fn = flow_signal_accumulation_op_sum;
        break;
    case SIG_ACCUM_LAST:
        attr->accumulation_op_fn = flow_signal_accumulation_op_last;
        break;
    case SIG_ACCUM_MIN:
        attr->accumulation_op_fn = flow_signal_accumulation_op_min;
        break;
    case SIG_ACCUM_MAX:
        attr->accumulation_op_fn = flow_signal_accumulation_op_max;
        break;
    case SIG_ACCUM_AVG:
        LOG_CRIT("[dev=%p conf=%p] average signal accumulation type is not "
                 "supported",
                 config->device, config);
        return ERROR;
    default:
        LOG_CRIT("[dev=%p conf=%p] unknown signal accumulation type requested",
                 config->device, config);
        return ERROR;
    }
    return SUCCESS;
}

int algorithm_config_signal_trigger_set(struct algorithm_config *config,
                                        size_t user_index, int threshold) {
    struct signal_attr *attr;
    ATTR_LIST_ITEM_SET(&config->signals_list, struct signal_attr, user_index,
                       threshold, attr);
    if (threshold <= 0)
        return ERROR;

    attr->is_trigger = true;
    attr->trigger_check_fn = flow_handler_trigger_check;
    return SUCCESS;
}

int algorithm_config_control_add(struct algorithm_config *config,
                                 control_t control, size_t user_index) {
    ATTR_LIST_DUPLICATE_USER_INDEX_CHK(&config->controls_list,
                                       struct control_attr, user_index);
    ATTR_LIST_DUPLICATE_TYPE_CHK(&config->controls_list, struct control_attr,
                                 control);
    struct control_attr *attr;
    ATTR_LIST_ITEM_ALLOC(&config->controls_list, user_index,
                         config->num_controls, ALGO_CONF_MAX_NUM_CONTROLS,
                         attr);
    attr->type = control;
    return SUCCESS;
}

int algorithm_config_control_initial_value_set(struct algorithm_config *config,
                                               size_t user_index,
                                               int initial_value) {
    struct control_attr *attr;
    ATTR_LIST_ITEM_SET(&config->controls_list, struct control_attr, user_index,
                       initial_value, attr);
    return SUCCESS;
}

int algorithm_config_local_state_add(struct algorithm_config *config,
                                     size_t user_index) {
    ATTR_LIST_DUPLICATE_USER_INDEX_CHK(&config->local_state_list,
                                       struct local_state_attr, user_index);
    struct local_state_attr *attr;
    ATTR_LIST_ITEM_ALLOC(&config->local_state_list, user_index,
                         config->num_local_states,
                         ALGO_CONF_MAX_LOCAL_STATE_VARS, attr);
    return SUCCESS;
}

int algorithm_config_local_state_set(struct algorithm_config *config,
                                     size_t user_index, int initial_value) {
    struct local_state_attr *attr;
    ATTR_LIST_ITEM_SET(&config->local_state_list, struct local_state_attr,
                       user_index, initial_value, attr);
    return SUCCESS;
}

int algorithm_config_compile(struct algorithm_config *config,
                             const char *compile_path, char **err) {
    config->dlopen_handle = dlopen(compile_path, RTLD_NOW | RTLD_LOCAL);
    if (!config->dlopen_handle) {
        LOG_CRIT("[dev=%p conf=%p] dlopen(%s) failed with %s", config->device,
                 config, compile_path, dlerror());
        *err = dlerror();
        return ERROR;
    }

    config->algorithm_fn = (algo_function_t)dlsym(
        config->dlopen_handle, __algorithm_entry_point_symbol);
    if (!config->algorithm_fn) {
        *err = dlerror();
        dlclose(config->dlopen_handle);
        LOG_CRIT("[dev=%p conf=%p] %s symbol lookup in %s failed with %s",
                 config->device, config, __algorithm_entry_point_symbol,
                 compile_path, dlerror());
        return ERROR;
    }

    LOG_DBG("[dev=%p conf=%p] loaded %s symbol=%p from file=%s", config->device,
            config, __algorithm_entry_point_symbol, config->algorithm_fn,
            compile_path);

    return SUCCESS;
}