#include "impl.h"

err_t register_pcmc(void *dev, addr_t src_addr, addr_mask_t src_addr_mask,
                    addr_t dst_addr, addr_mask_t dst_addr_mask,
                    handle_t *handle) {
    int ret = SUCCESS;
    struct algorithm_config *new_config;

    ret = algorithm_config_alloc((device_t *)dev, &new_config);
    if (ret != SUCCESS)
        return ret;

    (void)src_addr;
    (void)dst_addr;
    (void)src_addr_mask;
    (void)dst_addr_mask;
    ret = algorithm_config_matching_rule_add(new_config, UINT32_MAX);
    if (ret != SUCCESS)
        algorithm_config_destroy(new_config);

    *handle = new_config;
    return ret;
}

err_t deregister_pcmc(handle_t handle) {
    return algorithm_config_destroy((struct algorithm_config *)handle);
}

err_t activate_pcmc(handle_t handle) {
    return algorithm_config_activate((struct algorithm_config *)handle);
}

err_t deactivate_pcmc(handle_t handle) {
    return algorithm_config_deactivate((struct algorithm_config *)handle);
}

err_t register_signal_pcmc(signal_t signal, signal_accum_t accum_type,
                           size_t user_index, handle_t handle) {
    return algorithm_config_signal_add((struct algorithm_config *)handle,
                                       signal, accum_type, user_index);
}

err_t register_signal_invoke_trigger_pcmc(size_t user_index, pcm_uint threshold,
                                          handle_t handle) {
    return algorithm_config_signal_trigger_set(
        (struct algorithm_config *)handle, user_index, threshold);
}

err_t register_control_pcmc(control_t control, size_t user_index,
                            handle_t handle) {
    return algorithm_config_control_add((struct algorithm_config *)handle,
                                        control, user_index);
}

err_t register_control_initial_value_pcmc(size_t user_index,
                                          pcm_uint initial_value,
                                          handle_t handle) {
    return algorithm_config_control_initial_value_set(
        (struct algorithm_config *)handle, user_index, initial_value);
}

err_t register_local_state_pcmc(size_t user_index, handle_t handle) {
    return algorithm_config_local_state_add((struct algorithm_config *)handle,
                                            user_index);
}

err_t register_local_state_initial_value_float_pcmc(size_t user_index,
                                                    pcm_float initial_value,
                                                    handle_t handle) {
    return algorithm_config_local_state_float_set(
        (struct algorithm_config *)handle, user_index, initial_value);
}

err_t register_local_state_initial_value_int_pcmc(size_t user_index,
                                                  pcm_int initial_value,
                                                  handle_t handle) {
    return algorithm_config_local_state_int_set(
        (struct algorithm_config *)handle, user_index, initial_value);
}

err_t register_local_state_initial_value_uint_pcmc(size_t user_index,
                                                   pcm_uint initial_value,
                                                   handle_t handle) {
    return algorithm_config_local_state_uint_set(
        (struct algorithm_config *)handle, user_index, initial_value);
}

err_t register_local_state_initial_value_pcmc(size_t user_index,
                                              pcm_uint initial_value,
                                              handle_t handle) {
    return register_local_state_initial_value_uint_pcmc(user_index,
                                                        initial_value, handle);
}

err_t register_constant_pcmc(size_t user_index, handle_t handle) {
    return algorithm_config_constant_add((struct algorithm_config *)handle,
                                         user_index);
}

err_t register_constant_value_float_pcmc(size_t user_index, pcm_float value,
                                         handle_t handle) {
    return algorithm_config_constant_float_set(
        (struct algorithm_config *)handle, user_index, value);
}

err_t register_constant_value_int_pcmc(size_t user_index, pcm_int value,
                                       handle_t handle) {
    return algorithm_config_constant_int_set((struct algorithm_config *)handle,
                                             user_index, value);
}

err_t register_constant_value_uint_pcmc(size_t user_index, pcm_uint value,
                                        handle_t handle) {
    return algorithm_config_constant_uint_set((struct algorithm_config *)handle,
                                              user_index, value);
}

err_t register_algorithm_pcmc(const char *compile_path,
                              char **compile_output_string, handle_t handle) {
    return algorithm_config_compile((struct algorithm_config *)handle,
                                    compile_path, compile_output_string);
}
