#ifndef _PCM_H_
#define _PCM_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
#define STATIC_ASSERT static_assert
#else
#define STATIC_ASSERT _Static_assert
#endif
STATIC_ASSERT(sizeof(double) == sizeof(uint64_t),
              "sizeof(double) must be equal to sizeof(uint64_t)");

typedef int64_t pcm_int;
typedef uint64_t pcm_uint;
typedef double pcm_float;

/**
 * @file pcm.h
 * @brief Public API for Programmable Congestion Management (PCM) library.
 *
 * This interface provides handle-based management for PCM instance (PCMI)
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
 * @enum err_t
 * @brief Return codes for PCM calls.
 */
typedef enum err {
    PCM_SUCCESS = 0, /**< Operation completed successfully */
    PCM_ERROR = 1    /**< Generic error occurred */
} pcm_err_t;

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
 * @enum pcm_signal_t
 * @brief Identifiers for PCM signals.
 */
typedef enum signal {
    PCM_SIG_ACK = 0,         /**< Number of ACK packets received */
    PCM_SIG_RTO = 1,         /**< Number of RTO packets received */
    PCM_SIG_NACK = 2,        /**< Number of NACK packets received */
    PCM_SIG_ECN = 3,         /**< Number of ECN packets received */
    PCM_SIG_RTT = 4,         /**< RTT timestamp */
    PCM_SIG_DATA_TX = 5,     /**< Number of sent bytes */
    PCM_SIG_DATA_NACKED = 6, /**< Number of sent bytes */
    PCM_SIG_IN_FLIGHT = 7,   /**< Number of in-flight bytes */
    PCM_SIG_ELAPSED_TIME = 8 /**< Monotonic elapsed time */
} pcm_signal_t;

/**
 * @enum pcm_signal_accum_t
 * @brief Accumulation operations for PCM signals.
 */
typedef enum signal_accum {
    PCM_SIG_ACCUM_SUM = 0, /**< Sum all samples */
    PCM_SIG_ACCUM_MIN = 1, /**< Keep minimum sample */
    PCM_SIG_ACCUM_MAX = 2, /**< Keep maximum sample */
    PCM_SIG_ACCUM_LAST = 3 /**< Keep only the last sample */
} pcm_signal_accum_t;

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
 * @enum pcm_control_t
 * @brief Identifiers for PCM control knobs.
 */
typedef enum control {
    PCM_CTRL_CWND = 0, /**< Sending congestion window */
    PCM_CTRL_RATE = 1  /**< Sending rate control */
} pcm_control_t;

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

/* Handler-side API */
#define ALGO_CTX_ARGS __attribute__((unused)) void *ctx
#define ALGO_CTX_PASS ctx
#define __algorithm_entry_point __algorithm_main(ALGO_CTX_ARGS)
#define __algorithm_entry_point_symbol "__algorithm_main"

#if defined(__GNUC__) || defined(__clang__)
#define PCM_FORCE_INLINE __attribute__((always_inline)) inline
#else
#define PCM_FORCE_INLINE inline
#endif

/**
 * @brief Get the current persistent state within a handler.
 *
 * Similar to get_var_uint.
 *
 * @param[in] idx   User-defined state index.
 * @return Current state value.
 */
#define get_var(idx) __flow_var_uint_get(ctx, idx)

/**
 * @brief Update the persistent state within a handler.
 *
 * Similar to set_var_int.
 *
 * @param[in] idx   User-defined state index.
 * @param[in] val          New state value.
 */
#define set_var(idx, val) __flow_var_uint_set(ctx, idx, val);

/**
 * @brief Get the integer current persistent state within a handler.
 *
 * @param[in] idx User-defined state index.
 * @return Current state integer value.
 */
#define get_var_int(idx) __flow_var_int_get(ctx, idx)

/**
 * @brief Update the integer persistent state within a handler.
 *
 * @param[in] idx   User-defined state index.
 * @param[in] val          New integer state value.
 */
#define set_var_int(idx, val) __flow_var_int_set(ctx, idx, val);

/**
 * @brief Get the unsigned integer current persistent state within a
 * handler.
 *
 * @param[in] idx User-defined state index.
 * @return Current state unsigned integer value.
 */
#define get_var_uint(idx) __flow_var_uint_get(ctx, idx)

/**
 * @brief Update the unsigned integer persistent state within a handler.
 *
 * @param[in] idx   User-defined state index.
 * @param[in] val          New unsigned integer state value.
 */
#define set_var_uint(idx, val) __flow_var_uint_set(ctx, idx, val);

/**
 * @brief Get the float current persistent state within a handler.
 *
 * @param[in] idx User-defined state index.
 * @return Current state float value.
 */
#define get_var_float(idx) __flow_var_float_get(ctx, idx)

/**
 * @brief Update the float persistent state within a handler.
 *
 * @param[in] idx   User-defined state index.
 * @param[in] val          New float state value.
 */
#define set_var_float(idx, val) __flow_var_float_set(ctx, idx, val);

/**
 * @brief Read the latest signal value within a handler.
 *
 * @param[in] idx   User-defined signal index.
 * @return Current signal value.
 */
#define get_signal(idx) __flow_signal_get(ctx, idx)

/**
 * @brief Set the signal value within a handler.
 *
 * @param[in] idx   User-defined signal index.
 * @param[in] val          New signal value.
 */
#define set_signal(idx, val) __flow_signal_set(ctx, idx, val)

#define PCM_SIG_REARM (UINT64_MAX - 1)

/**
 * @brief Update the signal value within a handler.
 *
 * @param[in] idx   User-defined signal index.
 * @param[in] val          Update value.
 */
#define update_signal(idx, val) __flow_signal_update(ctx, idx, val)

/**
 * @brief Get mask of signals that triggered handler.
 *
 * @return[in] trigger_mask with bits set for signals that triggered handler.
 */
#define get_signal_trigger_mask() __flow_signal_trigger_mask_get(ctx)

/**
 * @brief Read the current control knob value within a handler.
 *
 * @param[in] idx   User-defined control index.
 * @return Current control value.
 */
#define get_control(idx) __flow_control_get(ctx, idx)

/**
 * @brief Update the control knob value within a handler.
 *
 * @param[in] idx   User-defined control index.
 * @param[in] val          New control value.
 */
#define set_control(idx, val) __flow_control_set(ctx, idx, val)

/**
 * @brief Algorithm handler entry point
 */
#define algorithm_main() __algorithm_entry_point

#ifdef __cplusplus
}
#endif

#endif /* _PCM_H_ */