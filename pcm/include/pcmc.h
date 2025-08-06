#ifndef _PCMC_H_
#define _PCMC_H_

#include "pcm.h"

/*
 * This interface provides handle-based management for PCMС
 * creation, such as registration of matching rules, signals, controls, and
 * state.
 */

/**
 * @typedef pcm_handle_t
 * @brief Opaque handle for a PCM context.
 */
typedef void *pcm_handle_t;

/**
 * @typedef pcm_addr_t
 * @brief Opaque representation of a network address.
 */
typedef uint32_t pcm_addr_t;

/**
 * @typedef pcm_addr_mask_t
 * @brief Opaque representation of a network address mask.
 */
typedef uint32_t pcm_addr_mask_t;

/**
 * @brief Allocate a new PCM handle that represents a PCM
 * configuration (PCMC). New handle is associated with a matching rule for PDC's
 * CCC selection.
 *
 * A PDC's CCC is instantiated by this PCMC if PDC's address has longest prefix
 * match with the matching rule specified in this function.
 *
 * @param[in] dev               Implementation-specific pointer to the network
 * context
 * @param[in] src_addr          PDC source address to match.
 * @param[in] src_addr_mask     Mask for wildcard PDC source address matching.
 * @param[in] dst_addr          PDC destination address to match.
 * @param[in] dst_addr_mask     Mask for wildcard PDC destination address
 * matching.
 * @param[out] handle            PCM handle.
 * @return SUCCESS on success, PCM_ERROR on failure.
 */
pcm_err_t register_pcmc(void *dev, pcm_addr_t src_addr,
                        pcm_addr_mask_t src_addr_mask, pcm_addr_t dst_addr,
                        pcm_addr_mask_t dst_addr_mask, pcm_handle_t *handle);

/**
 * @brief Release and free a previously allocated PCMC handle.
 *
 * @param[in] handle  Handle to release.
 * @return SUCCESS on success, PCM_ERROR on failure.
 */
pcm_err_t deregister_pcmc(pcm_handle_t handle);

/**
 * @brief Compile and register an algorithm with PCMC.
 *
 * @param[in] algo_name Name of the algorithm. LD_LIBRARY_PATH must
 * contain path to the lib<algo_name>.so and lib<algo_name>_pcmc.so.
 * @param[in] handle       Handle to associate the compiled algorithm with.
 * @return SUCCESS on success, PCM_ERROR on failure.
 */
pcm_err_t register_algorithm_pcmc(const char *algo_name, pcm_handle_t handle);

/**
 * @brief Activate the PCMC handle, enabling PCM-based management on new PDCs
 * that satisfy PCMC's matching rule.
 *
 * @param[in] handle  Handle to activate.
 * @return SUCCESS on success, PCM_ERROR on failure.
 */
pcm_err_t activate_pcmc(pcm_handle_t handle);

/**
 * @brief Deactivate the PCMC handle, disabling PCM-based management on new PDCs
 * that satisfy PCMC's matching rule.
 *
 * @param[in] handle  Handle to deactivate.
 * @return SUCCESS on success, PCM_ERROR on failure.
 */
pcm_err_t deactivate_pcmc(pcm_handle_t handle);

/**
 * @brief Register a signal for network events monitoring.
 *
 * @param[in] signal       Signal identifier (SIG_*).
 * @param[in] accum_type   Accumulation operation (SIG_ACCUM_*).
 * @param[in] idx   Index for user-defined signal mapping.
 * @param[in] handle       PCM handle.
 * @return SUCCESS on success, PCM_ERROR on failure.
 */
pcm_err_t register_signal_pcmc(pcm_signal_t signal,
                               pcm_signal_accum_t accum_type, size_t idx,
                               pcm_handle_t handle);

/**
 * @brief Set a threshold to trigger algorithm callback.
 *
 * When the monitored value for the signal at a idx exceeds the
 * threshold, the algorithm handler registered in PCMC will be invoked in the
 * CCC of PDC.
 *
 * @param[in] idx   User-defined signal index.
 * @param[in] threshold    Threshold value for triggering.
 * @param[in] handle       PCM handle.
 * @return SUCCESS on success, PCM_ERROR on failure.
 */
pcm_err_t register_signal_invoke_trigger_pcmc(size_t idx, pcm_uint threshold,
                                              pcm_handle_t handle);

/**
 * @brief Register a control knob for external adjustment by an algorithm.
 *
 * @param[in] control      Control identifier (CTRL_*).
 * @param[in] idx   Index for user-defined control mapping.
 * @param[in] handle       PCM handle.
 * @return SUCCESS on success, PCM_ERROR on failure.
 */
pcm_err_t register_control_pcmc(pcm_control_t control, size_t idx,
                                pcm_handle_t handle);

/**
 * @brief Set the initial value for an algorithm output control knob.
 *
 * This sets the starting value before activation.
 * Similar to the register_control_initial_value_int_pcmc.
 *
 * @param[in] idx   User-defined control index.
 * @param[in] initial_value Initial control value.
 * @param[in] handle       PCM handle.
 * @return SUCCESS on success, PCM_ERROR on failure.
 */
pcm_err_t register_control_initial_value_pcmc(size_t idx,
                                              pcm_uint initial_value,
                                              pcm_handle_t handle);

/**
 * @brief Register persistent integer value state storage for a flow.
 *
 * @param[in] idx   Index for user-defined state mapping.
 * @param[in] handle       PCM handle.
 * @return SUCCESS on success, PCM_ERROR on failure.
 */
pcm_err_t register_var_pcmc(size_t idx, pcm_handle_t handle);

/**
 * @brief Set the initial persistent state value for a flow.
 * Similar to register_var_initial_value_int_pcmc
 *
 * @param[in] idx   User-defined state index.
 * @param[in] initial_value Initial state value.
 * @param[in] handle       PCM handle.
 * @return SUCCESS on success, PCM_ERROR on failure.
 */
pcm_err_t register_var_initial_value_pcmc(size_t idx, pcm_uint initial_value,
                                          pcm_handle_t handle);

/**
 * @brief Set the initial persistent int state value for a flow.
 *
 * @param[in] idx   User-defined state index.
 * @param[in] initial_value Initial state value.
 * @param[in] handle       PCM handle.
 * @return SUCCESS on success, PCM_ERROR on failure.
 */
pcm_err_t register_var_initial_value_int_pcmc(size_t idx, pcm_int initial_value,
                                              pcm_handle_t handle);

/**
 * @brief Set the initial persistent uint state value for a flow.
 *
 * @param[in] idx   User-defined state index.
 * @param[in] initial_value Initial state value.
 * @param[in] handle       PCM handle.
 * @return SUCCESS on success, PCM_ERROR on failure.
 */
pcm_err_t register_var_initial_value_uint_pcmc(size_t idx,
                                               pcm_uint initial_value,
                                               pcm_handle_t handle);

/**
 * @brief Set the initial persistent float state value for a flow.
 *
 * @param[in] idx   User-defined state index.
 * @param[in] initial_value Initial state value.
 * @param[in] handle       PCM handle.
 * @return SUCCESS on success, PCM_ERROR on failure.
 */
pcm_err_t register_var_initial_value_float_pcmc(size_t idx,
                                                pcm_float initial_value,
                                                pcm_handle_t handle);

#endif /* _PCMC_H_ */