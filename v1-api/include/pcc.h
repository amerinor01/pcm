#ifndef PCC_H
#define PCC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/**
 * Opaque handle for a PCC device context.
 */
struct pcc_dev_ctx;

/**
 * Opaque handle for a PCC algorithm handler instance.
 */
struct pcc_algorithm_handler_obj;

/**
 * Opaque handle for an algorithm per-flow context.
 */
struct pcc_flow_ctx;

/**
 * @enum pcc_error_codes
 * @brief Return codes for PCC API functions.
 */
enum pcc_error_codes {
    PCC_SUCCESS = 0, /**< Operation completed successfully */
    PCC_ERROR = 1    /**< Generic error occurred */
};

/** @name Signal identifiers
 *  Bit‐flags for selecting which signals to work with.
 *  @{
 */
#define PCC_SIG_TYPE_CLOCK (1U << 0)       /**< Monotonic clock timestamp */
#define PCC_SIG_TYPE_CWND (1U << 1)        /**< Congestion window size */
#define PCC_SIG_TYPE_RATE (1U << 2)        /**< Sending rate */
#define PCC_SIG_TYPE_PACER_DELAY (1U << 3) /**< Packet pacing delay */
#define PCC_SIG_TYPE_RTT (1U << 4)         /**< Round-trip time */
#define PCC_SIG_TYPE_ACKED_BYTES (1U << 5) /**< Bytes acknowledged */
#define PCC_SIG_TYPE_ACKS_RECVD (1U << 6)  /**< ACK packets received */
#define PCC_SIG_TYPE_NACKS_RECVD (1U << 7) /**< NACK packets received */
#define PCC_SIG_TYPE_ECNS_RECVD (1U << 8)  /**< ECN-marked packets received */
#define PCC_SIG_TYPE_TRIM_RECVD (1U << 9)  /**< Trimmed packets received */
#define PCC_SIG_TYPE_RTOS (1U << 10)       /**< RTOs happpened */
/** @} */

/** @name Coalescing operations
 *  Specifies how to coalesce samples between resets.
 *  @{
 */
#define PCC_SIG_COALESCING_OP_ACCUM (1U << 0) /**< Accumulate (sum) all samples */
#define PCC_SIG_COALESCING_OP_LAST (1U << 1) /**< Keep only last sample */
#define PCC_SIG_COALESCING_OP_MIN (1U << 2)  /**< Compute minimum */
#define PCC_SIG_COALESCING_OP_MAX (1U << 3)  /**< Compute maximum */
#define PCC_SIG_COALESCING_OP_AVG (1U << 4)  /**< Compute running average */
/** @} */

/** @name Reset types
 *  Defines when a signal should be reset.
 *  @{
 */
#define PCC_SIG_RESET_TYPE_NONE (1U << 0) /**< Never reset */
#define PCC_SIG_RESET_TYPE_USR (1U << 1) /**< Reset by user call in flow handler */
#define PCC_SIG_RESET_TYPE_HANDLER_COMPL (1U << 2) /**< Reset upon every flow handler completion */
/** @} */

/** @name Permission flags
 *  Device vs user permissions for signals.
 *  @{
 */
#define PCC_SIG_READ (1U << 0)  /**< Read permission */
#define PCC_SIG_WRITE (1U << 1) /**< Write permission */
/** @} */

/** @name Data types
 *  Specifies the storage type of a signal.
 *  @{
 */
#define PCC_SIG_DTYPE_UINT64 (1U << 0) /**< Unsigned 64-bit integer */
#define PCC_SIG_DTYPE_FP64 (1U << 1)   /**< 64-bit IEEE floating point */
/** @} */

/**
 * @struct pcc_signal_attr
 * @brief Attributes defining a single PCC signal.
 */
struct pcc_signal_attr {
    uint32_t type;          /**< Signal type (PCC_SIG_TYPE_*) */
    uint32_t coalescing_op; /**< Coalescing operation to apply (PCC_SIG_COALESCING_OP_*) */
    uint32_t reset_type; /**< When to reset the signal (PCC_SIG_RESET_TYPE_*) */
    uint32_t dev_perms;  /**< Device‐side permissions (PCC_SIG_READ/WRITE) */
    uint32_t user_perms; /**< User‐side permissions (PCC_SIG_READ/WRITE) */
    uint32_t dtype;      /**< Data type of the signal (PCC_SIG_DTYPE_*) */
    uint64_t init_value; /**< Initial value set upon flow creation */
};

/**
 * @struct pcc_algorithm_handler_attrs
 * @brief Attributes for initializing a PCC flow handler.
 */
struct pcc_algorithm_handler_attrs {
    uint32_t signals_mask; /**< Mask of supported/requested signals */
    struct pcc_signal_attr *signal_attrs; /**< Array of signal attribute descriptors */
    size_t num_signals;         /**< Number of entries in signal_attrs */
    const void *user_data_init; /**< Intial contents of user init data */
    size_t user_data_size;      /**< Bytes to allocate for user data per flow */
    int (*cc_handler_fn)(struct pcc_flow_ctx *ctx); /**< Congestion control handler callback */
};

/**
 * @brief Initialize a PCC device context.
 *
 * @param[in]  dev_name  Null-terminated name of the PCC device.
 * @param[out] ctx       Pointer to receive the allocated device context handle.
 * @return PCC_SUCCESS on success, PCC_ERROR on failure.
 */
int pcc_dev_ctx_init(const char *dev_name, struct pcc_dev_ctx **ctx);

/**
 * @brief Destroy a previously initialized PCC device context.
 *
 * @param[in] ctx  Device context handle to destroy.
 * @return PCC_SUCCESS on success, PCC_ERROR on failure.
 */
int pcc_dev_ctx_destroy(struct pcc_dev_ctx *ctx);

/**
 * @brief Query the capabilities (signal attributes) of a PCC device.
 *
 * @param[in]  ctx   Device context handle.
 * @param[out] caps  Pointer to receive array of flow handler attribute sets.
 * @return PCC_SUCCESS on success, PCC_ERROR on failure.
 */
int pcc_dev_ctx_caps_query(struct pcc_dev_ctx *ctx,
                           struct pcc_algorithm_handler_attrs **caps);

/**
 * @brief Free the capabilities array returned by pcc_dev_ctx_caps_query().
 *
 * @param[in] caps  Array of flow handler attribute sets to free.
 */
int pcc_dev_ctx_caps_free(struct pcc_algorithm_handler_attrs *caps);

/**
 * @brief Instantiate a PCC algorithm handler on a device.
 *
 * @param[in] ctx            Device context handle.
 * @param[in] attrs          Attributes describing signals, user data size, and
 * handler.
 * @param[out] flow_handler  Pointer to a new flow handler object, or NULL on
 * failure.
 * @return PCC_SUCCESS on success, PCC_ERROR on failure.
 */
int pcc_algorithm_handler_install(
    struct pcc_dev_ctx *ctx, struct pcc_algorithm_handler_attrs *attrs,
    struct pcc_algorithm_handler_obj **flow_handler);

/**
 * @brief Destroy a PCC algorithm handler instance.
 *
 * @param[in] handle  Flow handler object to destroy.
 * @return PCC_SUCCESS on success, PCC_ERROR on failure.
 */
int pcc_algorithm_handler_remove(struct pcc_algorithm_handler_obj *handle);

/**
 * @brief Read a uint64_t signal value.
 *
 * @param[in] ctx         Flow context passed to the handler function.
 * @param[in] signal_idx  Index of the signal to read.
 * @return Current value of the signal.
 */
uint64_t pcc_flow_signal_read_u64(struct pcc_flow_ctx *ctx, size_t signal_idx);

/**
 * @brief Write a uint64_t given value to the signal storage.
 *
 * @param[in] ctx         Flow context passed to the handler function.
 * @param[in] signal_idx  Index of the signal to reset.
 * @param[in] val         Value to set the signal to.
 */
void pcc_flow_signal_write_u64(struct pcc_flow_ctx *ctx, size_t signal_idx,
                               uint64_t val);

/**
 * @brief Read an FP64 signal value.
 *
 * @param[in] ctx         Flow context passed to the handler function.
 * @param[in] signal_idx  Index of the signal to read.
 * @return Current value of the signal.
 */
double pcc_flow_signal_read_f64(struct pcc_flow_ctx *ctx, size_t signal_idx);

/**
 * @brief Write a given FP64 value to the signal storage.
 *
 * @param[in] ctx         Flow context passed to the handler function.
 * @param[in] signal_idx  Index of the signal to reset.
 * @param[in] val         Value to set the signal to.
 */
void pcc_flow_signal_write_f64(struct pcc_flow_ctx *ctx, size_t signal_idx,
                               double val);

/**
 * @brief Retrieve pointer to user data for the current flow.
 *
 * @param[in] ctx  Flow context passed to the handler function.
 * @return Pointer to the user data region (size defined in handler attrs).
 */
void *pcc_flow_user_data_ptr_get(struct pcc_flow_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif /* PCC_H */