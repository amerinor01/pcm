#ifndef PCC_H
#define PCC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/**
 * Opaque handle for a PCC device context.
 */
struct pcc_dev_ctx;

/**
 * Opaque handle for a PCC flow handler instance.
 */
struct pcc_flow_handler_obj;

/**
 * @enum pcc_error_codes
 * @brief Return codes for PCC API functions.
 */
enum pcc_error_codes
{
    PCC_SUCCESS = 0, /**< Operation completed successfully */
    PCC_ERROR   = 1  /**< Generic error occurred */
};

/** @name Signal identifiers
 *  Bit‐flags for selecting which signals to work with.
 *  @{
 */
#define PCC_SIG_CLOCK       (1U << 0) /**< Monotonic clock timestamp */
#define PCC_SIG_CWND        (1U << 1) /**< Congestion window size */
#define PCC_SIG_RATE        (1U << 2) /**< Sending rate */
#define PCC_SIG_PACER_DELAY (1U << 3) /**< Packet pacing delay */
#define PCC_SIG_RTT         (1U << 4) /**< Round-trip time */
#define PCC_SIG_ACKED_BYTES (1U << 5) /**< Bytes acknowledged */
#define PCC_SIG_ACKS_RECVD  (1U << 6) /**< ACK packets received */
#define PCC_SIG_NACKS_RECVD (1U << 7) /**< NACK packets received */
#define PCC_SIG_ECNS_RECVD  (1U << 8) /**< ECN-marked packets received */
/** @} */

/** @name Coalescing operations
 *  Specifies how to coalesce samples between reads/resets.
 *  @{
 */
#define PCC_SIG_COALESCING_OP_ACCUM (1U << 0) /**< Accumulate all samples */
#define PCC_SIG_COALESCING_OP_LAST  (1U << 1) /**< Keep only last sample */
#define PCC_SIG_COALESCING_OP_MIN   (1U << 2) /**< Compute minimum */
#define PCC_SIG_COALESCING_OP_MAX   (1U << 3) /**< Compute maximum */
#define PCC_SIG_COALESCING_OP_AVG   (1U << 4) /**< Compute average */
/** @} */

/** @name Reset types
 *  Defines when a signal should be reset.
 *  @{
 */
#define PCC_SIG_RESET_TYPE_NONE          (1U << 0) /**< Never reset */
#define PCC_SIG_RESET_TYPE_USR           (1U << 1) /**< Reset by user call */
#define PCC_SIG_RESET_TYPE_HANDLER_COMPL (1U << 2) /**< Reset when handler completes */
/** @} */

/** @name Permission flags
 *  Device vs user permissions for signals.
 *  @{
 */
#define PCC_SIG_READ  (1U << 0) /**< Read permission */
#define PCC_SIG_WRITE (1U << 1) /**< Write permission */
/** @} */

/** @name Data types
 *  Specifies the storage type of a signal.
 *  @{
 */
#define PCC_SIG_DTYPE_UINT64 (1U << 0) /**< Unsigned 64-bit integer */
#define PCC_SIG_DTYPE_FP64   (1U << 1) /**< 64-bit IEEE floating point */
/** @} */

/**
 * @struct pcc_signal_attr
 * @brief Attributes defining a single PCC signal.
 */
struct pcc_signal_attr
{
    uint32_t type;           /**< Bitmask of PCC_SIG_* identifiers to select the signal */
    uint32_t coalescing_op;  /**< Coalescing operation to apply (PCC_SIG_COALESCING_OP_*) */
    uint32_t reset_type;     /**< When to reset the signal (PCC_SIG_RESET_TYPE_*) */
    uint32_t dev_perms;      /**< Device‐side permissions (PCC_SIG_READ/WRITE) */
    uint32_t user_perms;     /**< User‐side permissions (PCC_SIG_READ/WRITE) */
    uint32_t dtype;          /**< Data type of the signal (PCC_SIG_DTYPE_*) */
    uint64_t init_value;     /**< Initial value upon flow creation */
};

/**
 * @struct pcc_flow_handler_attrs
 * @brief Attributes for initializing a PCC flow handler.
 */
struct pcc_flow_handler_attrs
{
    struct pcc_signal_attr *signal_attrs;    /**< Array of signal attribute descriptors */
    size_t num_signals;                      /**< Number of entries in signal_attrs */
    size_t user_data_size;                   /**< Bytes to allocate for user data per flow */
    int (*pcc_fn)(struct pcc_flow_ctx *ctx); /**< Congestion control handler callback */
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
int pcc_dev_ctx_caps_query(struct pcc_dev_ctx *ctx, struct pcc_flow_handler_attrs **caps);

/**
 * @brief Free the capabilities array returned by pcc_dev_ctx_caps_query().
 *
 * @param[in] caps  Array of flow handler attribute sets to free.
 */
int pcc_dev_ctx_caps_free(struct pcc_flow_handler_attrs *caps);
 
/**
 * @brief Instantiate a PCC flow handler on a device.
 *
 * @param[in] ctx    Device context handle.
 * @param[in] attrs  Attributes describing signals, user data size, and handler.
 * @return Pointer to a new flow handler object, or NULL on failure.
 */
struct pcc_flow_handler_obj *pcc_flow_handler_init(struct pcc_dev_ctx *ctx, struct pcc_flow_handler_attrs *attrs);

/**
 * @brief Destroy a PCC flow handler instance.
 *
 * @param[in] handle  Flow handler object to destroy.
 * @return PCC_SUCCESS on success, PCC_ERROR on failure.
 */
int pcc_flow_handler_destroy(struct pcc_flow_handler_obj *handle);

/**
 * @brief Read a uint64_t floating-point signal value.
 *
 * @param[in] ctx         Flow context passed to the handler function.
 * @param[in] signal_idx  Index of the signal to read.
 * @return Current value of the signal.
 */
uint64_t pcc_flow_signal_read_u64(struct pcc_flow_ctx *ctx, size_t signal_idx);

/**
 * @brief Reset a uint64_t signal to a given initial value.
 *
 * @param[in] ctx         Flow context passed to the handler function.
 * @param[in] signal_idx  Index of the signal to reset.
 * @param[in] init_val    Value to set the signal to.
 */
void pcc_flow_signal_reset_u64(struct pcc_flow_ctx *ctx, size_t signal_idx, uint64_t init_val);

/**
 * @brief Read a FP64-precision floating-point signal value.
 *
 * @param[in] ctx         Flow context passed to the handler function.
 * @param[in] signal_idx  Index of the signal to read.
 * @return Current value of the signal.
 */
double pcc_flow_signal_read_f64(struct pcc_flow_ctx *ctx, size_t signal_idx);

/**
 * @brief Reset a FP64-point signal to a given initial value.
 *
 * @param[in] ctx         Flow context passed to the handler function.
 * @param[in] signal_idx  Index of the signal to reset.
 * @param[in] init_val    Value to set the signal to.
 */
void pcc_flow_signal_reset_f64(struct pcc_flow_ctx *ctx, size_t signal_idx, double init_val);

/**
 * @brief Retrieve pointer to user data for the current flow.
 *
 * @param[in] ctx  Flow context passed to the handler function.
 * @return Pointer to the user data region (size defined in handler attrs).
 */
void *pcc_flow_user_data_get(struct pcc_flow_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif /* PCC_H */