#ifndef _PCM_H_
#define _PCM_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file pcm.h
 * @brief Public API for Programmable Congestion Management (PCM) library.
 *
 * This interface provides handle-based management for PCM instance (PCMI)
 * creation, such as registration of matching rules, signals, controls, and
 * state.
 */

/**
 * @typedef handle_t
 * @brief Opaque handle for a PCM context.
 */
typedef void *handle_t;

/**
 * @typedef addr_t
 * @brief Opaque representation of a network address.
 */
typedef uint32_t addr_t;

/**
 * @typedef addr_mask_t
 * @brief Opaque representation of a network address mask.
 */
typedef uint32_t addr_mask_t;

/**
 * @enum err_t
 * @brief Return codes for PCM calls.
 */
typedef enum err {
    SUCCESS = 0, /**< Operation completed successfully */
    ERROR = 1    /**< Generic error occurred */
} err_t;

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
 * @return SUCCESS on success, ERROR on failure.
 */
err_t register_pcmc(void *dev, addr_t src_addr, addr_mask_t src_addr_mask,
                    addr_t dst_addr, addr_mask_t dst_addr_mask,
                    handle_t *handle);

/**
 * @brief Release and free a previously allocated PCMC handle.
 *
 * @param[in] handle  Handle to release.
 * @return SUCCESS on success, ERROR on failure.
 */
err_t deregister_pcmc(handle_t handle);

/**
 * @brief Activate the PCMC handle, enabling PCM-based management on new PDCs
 * that satisfy PCMC's matching rule.
 *
 * @param[in] handle  Handle to activate.
 * @return SUCCESS on success, ERROR on failure.
 */
err_t activate_pcmc(handle_t handle);

/**
 * @brief Deactivate the PCMC handle, disabling PCM-based management on new PDCs
 * that satisfy PCMC's matching rule.
 *
 * @param[in] handle  Handle to deactivate.
 * @return SUCCESS on success, ERROR on failure.
 */
err_t deactivate_pcmc(handle_t handle);

/**
 * @enum signal_t
 * @brief Identifiers for PCM signals.
 */
typedef enum signal {
    SIG_ACK = 0,         /**< Number of ACK packets received */
    SIG_RTO = 1,         /**< Number of RTO packets received */
    SIG_NACK = 2,        /**< Number of NACK packets received */
    SIG_ELAPSED_TIME = 3 /**< Monotonic elapsed time */
} signal_t;

/**
 * @enum signal_accum_t
 * @brief Accumulation operations for PCM signals.
 */
typedef enum signal_accum {
    SIG_ACCUM_SUM = 0,  /**< Sum all samples */
    SIG_ACCUM_MIN = 1,  /**< Keep minimum sample */
    SIG_ACCUM_MAX = 2,  /**< Keep maximum sample */
    SIG_ACCUM_LAST = 3, /**< Keep only the last sample */
    SIG_ACCUM_AVG = 4   /**< Compute running average across all samples */
} signal_accum_t;

/**
 * @brief Register a signal for network events monitoring.
 *
 * @param[in] signal       Signal identifier (SIG_*).
 * @param[in] accum_type   Accumulation operation (SIG_ACCUM_*).
 * @param[in] user_index   Index for user-defined signal mapping.
 * @param[in] handle       PCM handle.
 * @return SUCCESS on success, ERROR on failure.
 */
err_t register_signal_pcmc(signal_t signal, signal_accum_t accum_type,
                           int user_index, handle_t handle);

/**
 * @brief Set a threshold to trigger algorithm callback.
 *
 * When the monitored value for the signal at a user_index exceeds the
 * threshold, the algorithm handler registered in PCMC will be invoked in the
 * CCC of PDC.
 *
 * @param[in] user_index   User-defined signal index.
 * @param[in] threshold    Threshold value for triggering.
 * @param[in] handle       PCM handle.
 * @return SUCCESS on success, ERROR on failure.
 */
err_t register_signal_invoke_trigger_pcmc(size_t user_index, int threshold,
                                          handle_t handle);

/**
 * @enum control_t
 * @brief Identifiers for PCM control knobs.
 */
typedef enum control {
    CTRL_CWND = 0, /**< Sending congestion window */
    CTRL_RATE = 1  /**< Sending rate control */
} control_t;

/**
 * @brief Register a control knob for external adjustment by an algorithm.
 *
 * @param[in] control      Control identifier (CTRL_*).
 * @param[in] user_index   Index for user-defined control mapping.
 * @param[in] handle       PCM handle.
 * @return SUCCESS on success, ERROR on failure.
 */
err_t register_control_pcmc(control_t control, size_t user_index,
                            handle_t handle);

/**
 * @brief Set the initial value for an algorithm output control knob.
 *
 * This sets the starting value before activation.
 *
 * @param[in] user_index   User-defined control index.
 * @param[in] initial_value Initial control value.
 * @param[in] handle       PCM handle.
 * @return SUCCESS on success, ERROR on failure.
 */
err_t register_control_initial_value_pcmc(size_t user_index, int initial_value,
                                          handle_t handle);

/**
 * @brief Register persistent integer value state storage for a flow.
 *
 * @param[in] user_index   Index for user-defined state mapping.
 * @param[in] handle       PCM handle.
 * @return SUCCESS on success, ERROR on failure.
 */
err_t register_local_state_pcmc(size_t user_index, handle_t handle);

/**
 * @brief Set the initial persistent state value for a flow.
 *
 * @param[in] user_index   User-defined state index.
 * @param[in] initial_value Initial state value.
 * @param[in] handle       PCM handle.
 * @return SUCCESS on success, ERROR on failure.
 */
err_t register_local_state_initial_value_pcmc(size_t user_index,
                                              int initial_value,
                                              handle_t handle);

/**
 * @brief Compile and register an algorithm with PCMC.
 *
 * @param[in] compile_path Path to the directory containing algorithm
 * definition.
 * @param[out] compile_output_string Implementation defined null-terminated
 *        string that contains compile-time outputs.
 * @param[in] handle       Handle to associate the compiled algorithm with.
 * @return SUCCESS on success, ERROR on failure.
 */
err_t register_algorithm_pcmc(const char *compile_path,
                              char **compile_output_string, handle_t handle);

/* Handler-side API */

#define __algorithm_entry_point __algorithm_main(void *ctx)
#define __algorithm_entry_point_symbol "__algorithm_main"
int __flow_control_get(const void *ctx, size_t user_index);
void __flow_control_set(void *ctx, size_t user_index, int val);
int __flow_signal_get(const void *ctx, size_t user_index);
void __flow_signal_set(void *ctx, size_t user_index, int val);
int __flow_local_state_get(const void *ctx, size_t user_index);
void __flow_local_state_set(void *ctx, size_t user_index, int val);

/**
 * @brief Get the current persistent state within a handler.
 *
 * @param[in] user_index   User-defined state index.
 * @return Current state value.
 */
#define get_local_state(user_index) __flow_local_state_get(ctx, user_index)

/**
 * @brief Update the persistent state within a handler.
 *
 * @param[in] user_index   User-defined state index.
 * @param[in] val          New state value.
 */
#define set_local_state(user_index, val)                                       \
    __flow_local_state_set(ctx, user_index, val);

/**
 * @brief Read the latest signal value within a handler.
 *
 * @param[in] user_index   User-defined signal index.
 * @return Current signal value.
 */
#define get_signal(user_index) __flow_signal_get(ctx, user_index)

/**
 * @brief Update the signal value within a handler.
 *
 * @param[in] user_index   User-defined signal index.
 * @param[in] val          New signal value.
 */
#define set_signal(user_index, val) __flow_signal_set(ctx, user_index, val)

/**
 * @brief Read the current control knob value within a handler.
 *
 * @param[in] user_index   User-defined control index.
 * @return Current control value.
 */
#define get_control(user_index) __flow_control_get(ctx, user_index)

/**
 * @brief Update the control knob value within a handler.
 *
 * @param[in] user_index   User-defined control index.
 * @param[in] val          New control value.
 */
#define set_control(user_index, val) __flow_control_set(ctx, user_index, val)

/**
 * @brief Algorithm handler entry point
 */
#define algorithm_main() __algorithm_entry_point

#ifdef __cplusplus
}
#endif

#endif /* _PCM_H_ */