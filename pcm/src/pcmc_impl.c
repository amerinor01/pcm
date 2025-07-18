#include "impl.h"

pcm_err_t register_pcmc(void *dev, pcm_addr_t src_addr,
                        pcm_addr_mask_t src_addr_mask, pcm_addr_t dst_addr,
                        pcm_addr_mask_t dst_addr_mask, pcm_handle_t *handle) {
    int ret = PCM_SUCCESS;
    struct algorithm_config *new_config;

    ret = algorithm_config_alloc((device_t *)dev, &new_config);
    if (ret != PCM_SUCCESS)
        return ret;

    (void)src_addr;
    (void)dst_addr;
    (void)src_addr_mask;
    (void)dst_addr_mask;
    ret = algorithm_config_matching_rule_add(new_config, UINT32_MAX);
    if (ret != PCM_SUCCESS)
        algorithm_config_destroy(new_config);

    *handle = new_config;
    return ret;
}

pcm_err_t deregister_pcmc(pcm_handle_t handle) {
    return algorithm_config_destroy((struct algorithm_config *)handle);
}

pcm_err_t activate_pcmc(pcm_handle_t handle) {
    return algorithm_config_activate((struct algorithm_config *)handle);
}

pcm_err_t deactivate_pcmc(pcm_handle_t handle) {
    return algorithm_config_deactivate((struct algorithm_config *)handle);
}

pcm_err_t register_signal_pcmc(pcm_signal_t signal,
                               pcm_signal_accum_t accum_type, size_t user_index,
                               pcm_handle_t handle) {
    return algorithm_config_signal_add((struct algorithm_config *)handle,
                                       signal, accum_type, user_index);
}

pcm_err_t register_signal_invoke_trigger_pcmc(size_t user_index,
                                              pcm_uint threshold,
                                              pcm_handle_t handle) {
    return algorithm_config_signal_trigger_set(
        (struct algorithm_config *)handle, user_index, threshold);
}

pcm_err_t register_control_pcmc(pcm_control_t control, size_t user_index,
                                pcm_handle_t handle) {
    return algorithm_config_control_add((struct algorithm_config *)handle,
                                        control, user_index);
}

pcm_err_t register_control_initial_value_pcmc(size_t user_index,
                                              pcm_uint initial_value,
                                              pcm_handle_t handle) {
    return algorithm_config_control_initial_value_set(
        (struct algorithm_config *)handle, user_index, initial_value);
}

pcm_err_t register_var_pcmc(size_t user_index, pcm_handle_t handle) {
    return algorithm_config_var_add((struct algorithm_config *)handle,
                                            user_index);
}

pcm_err_t register_var_initial_value_float_pcmc(size_t user_index,
                                                        pcm_float initial_value,
                                                        pcm_handle_t handle) {
    return algorithm_config_var_float_set(
        (struct algorithm_config *)handle, user_index, initial_value);
}

pcm_err_t register_var_initial_value_int_pcmc(size_t user_index,
                                                      pcm_int initial_value,
                                                      pcm_handle_t handle) {
    return algorithm_config_var_int_set(
        (struct algorithm_config *)handle, user_index, initial_value);
}

pcm_err_t register_var_initial_value_uint_pcmc(size_t user_index,
                                                       pcm_uint initial_value,
                                                       pcm_handle_t handle) {
    return algorithm_config_var_uint_set(
        (struct algorithm_config *)handle, user_index, initial_value);
}

pcm_err_t register_var_initial_value_pcmc(size_t user_index,
                                                  pcm_uint initial_value,
                                                  pcm_handle_t handle) {
    return register_var_initial_value_uint_pcmc(user_index,
                                                        initial_value, handle);
}

pcm_err_t register_algorithm_pcmc(const char *algo_name, pcm_handle_t handle) {
    return algorithm_config_compile((struct algorithm_config *)handle,
                                    algo_name);
}
