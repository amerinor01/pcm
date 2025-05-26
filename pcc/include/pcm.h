#ifndef PCM_H
#define PCM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/**
 * Opaque handle for a PCM device context.
 */
struct pcm_dev_ctx;

/**
 * Opaque handle for a PCM algorithm handler instance.
 */
struct pcm_algorithm_handler_obj;

/**
 * Opaque handle for an algorithm per-flow context.
 */
struct pcm_ccc_ctx;

/**
 * @enum pcm_error_codes
 * @brief Return codes for PCM API functions.
 */
enum pcm_error_codes {
    PCM_SUCCESS = 0, /**< Operation completed successfully */
    PCM_ERROR = 1    /**< Generic error occurred */
};

/** @name Data types
 *  Specifies the storage type of a signal/control knob.
 *  @{
 */
#define PCM_DTYPE_UINT64 (1U << 0) /**< Unsigned 64-bit integer */
/** @} */

/** @name Signal identifiers
 *  Bit‐flags for selecting which signals to work with.
 *  @{
 */
#define PCM_SIG_TYPE_ELAPSED_TIME (1U << 0) /**< Monotonic elapsed time */
#define PCM_SIG_TYPE_RTT (1U << 1)         /**< Round-trip time */
#define PCM_SIG_TYPE_DATA_ACKED_SIZE (1U << 2) /**< Size of acknowledged data */
#define PCM_SIG_TYPE_DATA_SENT_SIZE (1U << 3) /**< Size of data sent into the network */
#define PCM_SIG_TYPE_DATA_RECVD_SIZE (1U << 4) /**< Size of data received from the network */
#define PCM_SIG_TYPE_ACKS_RECVD (1U << 5)  /**< ACK packets received */
#define PCM_SIG_TYPE_NACKS_RECVD (1U << 6) /**< NACK packets received */
#define PCM_SIG_TYPE_ECNS_RECVD (1U << 7)  /**< ECN-marked packets received */
#define PCM_SIG_TYPE_TRIM_RECVD (1U << 8)  /**< Trimmed packets received */
#define PCM_SIG_TYPE_RTOS (1U << 9)       /**< RTOs happpened */
/** @} */

/** @name Accumulate operations
 *  Specifies how to coalesce samples between resets.
 *  @{
 */
#define PCM_SIG_ACCUMULATE_OP_SUM (1U << 0) /**< Sum all samples */
#define PCM_SIG_ACCUMULATE_OP_LAST (1U << 1) /**< Keep only last sample */
#define PCM_SIG_ACCUMULATE_OP_MIN (1U << 2)  /**< Compute minimum */
#define PCM_SIG_ACCUMULATE_OP_MAX (1U << 3)  /**< Compute maximum */
#define PCM_SIG_ACCUMULATE_OP_AVG (1U << 4)  /**< Compute running average */
/** @} */

/** @name Reset types
 *  Defines when a signal should be reset.
 *  @{
 */
#define PCM_SIG_RESET_TYPE_NONE (1U << 0) /**< Never reset */
#define PCM_SIG_RESET_TYPE_USR (1U << 1) /**< Reset by user call in algorithm handler */
#define PCM_SIG_RESET_TYPE_HANDLER_COMPL (1U << 2) /**< Reset upon every algorithm handler completion */
/** @} */

/**
 * @struct pcm_signal_attr
 * @brief Attributes defining a single PCM input signal.
 */
struct pcm_signal_attr {
    uint32_t type;          /**< Signal type (PCM_SIG_TYPE_*) */
    uint32_t accumulate_op; /**< Accumulate operation to apply (PCM_SIG_ACCUMULATE_OP_*) */
    uint32_t reset_type; /**< When to reset the signal (PCM_SIG_RESET_TYPE_*) */
    uint32_t dtype;      /**< Data type of the signal (PCM_DTYPE_*) */
    size_t size; /**< Width of the signal (number of entries in signal vector) */
    bool is_trigger; /**< If set, signal will trigger algorithm handler execution upon reaching non-zero threshold */
    uint64_t *thresholds; /**< Initial trigger thresholds set for each entry */
};

/** @name Control identifiers
 *  Bit‐flags for selecting which control knobs to work with.
 *  @{
 */
#define PCM_CTRL_TYPE_CWND (1U << 0)        /**< Congestion window size */
#define PCM_CTRL_TYPE_RATE (1U << 1)        /**< Sending rate */
#define PCM_CTRL_TYPE_PACER_DELAY (1U << 2) /**< Packet pacing delay */
#define PCM_CTRL_TYPE_EV (1U << 3)          /**< Flow entropy value */
/** @} */

struct pcm_control_attr {
    uint32_t type; /**< Signal type (PCM_CTRL_TYPE_*) */
    uint32_t dtype; /**< Data type of the signal (PCM_DTYPE_*) */
    size_t size; /**< Width of the control signal (number of entries in control vector) */
    uint64_t *init_values; /**< Initial values set for each entry upon CCC creation */
};

/** @name Algorithm handler capabilities
 *  Bit‐flags for selecting .
 *  @{
 */
#define PCM_AH_USER_DATA (1U << 0) /**< Algorithm handler needs per-CCC user data */
#define PCM_AH_SCHED_BEST_EFFORT (1U << 1) /**< Algorithm handler is invoked when compute capacity is available */
#define PCM_AH_TRIGGER (1U << 2) /**< Algorithm handler is invoked upon any of its signals reached a trigger threshold */
/** @} */

/**
 * @struct pcm_algorithm_handler_attrs
 * @brief Attributes for initializing a PCM algorithm handler.
 */
struct pcm_algorithm_handler_attrs {
    struct signal_attrs {
        uint32_t mask; /**< Mask of supported/requested input signals */
        size_t size; /**< Number of entries in signal_attrs */
        struct pcm_signal_attr *attrs; /**< Array of signal attribute descriptors */
    } signals;
    struct control_attrs {
        uint32_t mask; /**< Mask of supported/requested output control */
        size_t size; /**< Number of entries in control_attrs */
        struct pcm_control_attr *attrs; /**< Array of control attribute descriptors */
    } control;
    const void *user_data_init; /**< Intial contents of user init data */
    size_t user_data_size;      /**< Bytes to allocate for user data per CCC */
    uint32_t mask; /**< Mask to identify algorithm handler attributes */
    int (*cc_handler_fn)(struct pcm_ccc_ctx *ctx); /**< Congestion control handler callback */
};

/**
 * @brief Initialize a PCM device context.
 *
 * @param[in]  dev_name  Null-terminated name of the PCM device.
 * @param[out] ctx       Pointer to receive the allocated device context handle.
 * @return PCM_SUCCESS on success, PCM_ERROR on failure.
 */
int pcm_dev_ctx_init(const char *dev_name, struct pcm_dev_ctx **ctx);

/**
 * @brief Destroy a previously initialized PCM device context.
 *
 * @param[in] ctx  Device context handle to destroy.
 * @return PCM_SUCCESS on success, PCM_ERROR on failure.
 */
int pcm_dev_ctx_destroy(struct pcm_dev_ctx *ctx);

/**
 * @brief Query the capabilities (signal attributes) of a PCM device.
 *
 * @param[in]  ctx   Device context handle.
 * @param[out] caps  Pointer to array of algorithm handler attributes.
 * @return PCM_SUCCESS on success, PCM_ERROR on failure.
 */
int pcm_dev_ctx_caps_query(struct pcm_dev_ctx *ctx,
                           struct pcm_algorithm_handler_attrs **caps);

/**
 * @brief Free the capabilities array returned by pcm_dev_ctx_caps_query().
 *
 * @param[in] caps  Array of flow handler attribute sets to free.
 */
int pcm_dev_ctx_caps_free(struct pcm_algorithm_handler_attrs *caps);

/**
 * @brief Instantiate a PCM algorithm handler on a device.
 *
 * @param[in] ctx            Device context handle.
 * @param[in] attrs          Attributes describing signals, user data size, and handler.
 * @param[out] flow_handler  Pointer to a new flow handler object, or NULL on failure.
 * @return PCM_SUCCESS on success, PCM_ERROR on failure.
 */
int pcm_algorithm_handler_install(
    struct pcm_dev_ctx *ctx, struct pcm_algorithm_handler_attrs *attrs,
    struct pcm_algorithm_handler_obj **algo_handler);

/**
 * @brief Destroy a PCM algorithm handler instance.
 *
 * @param[in] handle  Flow handler object to destroy.
 * @return PCM_SUCCESS on success, PCM_ERROR on failure.
 */
int pcm_algorithm_handler_remove(struct pcm_algorithm_handler_obj *handle);

/**
 * @brief Read a uint64_t signal value.
 *
 * @param[in] ctx         CCC passed to the handler function.
 * @param[in] signal_idx  Index of the signal to read.
 * @param[in] entry_idx   Index of the entry in the signal to read.
 * @return Current value of the signal.
 */
uint64_t pcm_ccc_signal_read_u64(struct pcm_ccc_ctx *ctx, size_t signal_idx,
                                 size_t entry_idx);

/**
 * @brief Reset a uint64_t value of signal entry.
 *
 * @param[in] ctx         CCC passed to the handler function.
 * @param[in] signal_idx  Index of the signal to reset.
 * @param[in] entry_idx   Index of the entry in the signal to reset.
 */
void pcm_ccc_signal_reset_u64(struct pcm_ccc_ctx *ctx, size_t signal_idx,
                              size_t entry_idx);

/**
 * @brief Set a uint64_t signal value threshold to trigger algorithm handler.
 *
 * @param[in] ctx         CCC passed to the handler function.
 * @param[in] ctrl_idx    Index of the control knob to set threshold on.
 * @param[in] entry_idx   Index of the entry in the control knob to set threshold for.
 * @param[in] threshold   Threshold to set.
 */
void pcm_ccc_signal_threshold_set_u64(struct pcm_ccc_ctx *ctx, size_t ctrl_idx,
                                      size_t entry_idx, uint64_t threshold);

/**
 * @brief Read an uint64_t value of control knob entry.
 *
 * @param[in] ctx         CCC passed to the handler function.
 * @param[in] signal_idx  Index of the signal to read.
 * @param[in] entry_idx   Index of the entry in the control knob to read.
 * @return Current value of the control knob entry.
 */
uint64_t pcm_ccc_control_read_u64(struct pcm_ccc_ctx *ctx,
                                  size_t ctrl_idx, size_t entry_idx);

/**
 * @brief Write a uint64_t given value to the control knob entry storage.
 *
 * @param[in] ctx         CCC passed to the handler function.
 * @param[in] ctrl_idx    Index of the control knob to write.
 * @param[in] entry_idx   Index of the entry in the control knob to write.
 * @param[in] val         Value to set.
 */
void pcm_ccc_control_set_u64(struct pcm_ccc_ctx *ctx, size_t ctrl_idx,
                             size_t entry_idx, uint64_t val);

/**
 * @brief Retrieve pointer to user data for the current flow.
 *
 * @param[in] ctx  CCC passed to the handler function.
 * @return Pointer to the user data region (size defined in handler attrs).
 */
void *pcm_ccc_user_data_ptr_get(struct pcm_ccc_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif /* PCM_H */