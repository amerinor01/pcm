#ifndef _PCM_H_
#define _PCM_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define STATIC_ASSERT static_assert
#else
#define STATIC_ASSERT _Static_assert
#endif

#if defined(__GNUC__) || defined(__clang__)
#define PCM_FORCE_INLINE __attribute__((always_inline)) inline
#else
#define PCM_FORCE_INLINE inline
#endif

#if defined(__GNUC__) || defined(__clang__)
#define UTIL_MASK_TO_ARR_IDX(mask) ((unsigned long)__builtin_ctz(mask))
#else
STATIC_ASSERT(
    0, "__builtin_ctz macro is defined only for GCC and Clang compilers");
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t pcm_int;
typedef double pcm_float;
typedef uint64_t pcm_uint;

#define PCM_THRESHOLD_UNSPEC UINT64_MAX

/**
 * @file pcm.h
 * @brief Public API definitions for Programmable Congestion Management (PCM)
 * library.
 */

/**
 * @enum err_t
 * @brief Return codes for PCM calls.
 */
typedef enum err {
    PCM_SUCCESS = 0, /**< Operation completed successfully */
    PCM_ERROR = 1    /**< Generic error occurred */
} pcm_err_t;

/**
 * @enum pcm_signal_t
 * @brief Identifiers for PCM signals.
 */
typedef enum signal {
    PCM_SIG_ACK = 0,               /**< Number of ACK packets received */
    PCM_SIG_RTO = 1,               /**< Number of RTO packets received */
    PCM_SIG_NACK = 2,              /**< Number of NACK packets received */
    PCM_SIG_ECN = 3,               /**< Number of ECN packets received */
    PCM_SIG_RTT = 4,               /**< RTT timestamp */
    PCM_SIG_DATA_TX = 5,           /**< Number of sent bytes */
    PCM_SIG_DATA_NACKED = 6,       /**< Number of sent bytes */
    PCM_SIG_IN_FLIGHT = 7,         /**< Number of in-flight bytes */
    PCM_SIG_ELAPSED_TIME = 8,      /**< Monotonic elapsed time */
    PCM_SIG_ACK_EV = 9,            /**< EV in the last non-ECN-marked ACK */
    PCM_SIG_ECN_EV = 10,           /**< EV in the last ECN-marked ACK */
    PCM_SIG_NACK_EV = 11,          /**< EV in the last NACK */
    PCM_SIG_TX_READY_PKTS = 12,    /**< Number of packets ready for injection */
    PCM_SIG_TX_BACKLOG_BYTES = 13, /**< Number of packets ready for injection */
    PCM_SIG_UNKNOWN = 14, /**< Signal unknown - used for error handling */
} pcm_signal_t;

#define PCM_SIG_REARM (UINT64_MAX - 1)
#define PCM_SIG_NO_TRIGGER (UINT64_MAX)

/**
 * @enum pcm_signal_accum_t
 * @brief Accumulation operations for PCM signals.
 */
typedef enum signal_accum {
    PCM_SIG_ACCUM_SUM = 0,    /**< Sum all samples */
    PCM_SIG_ACCUM_MIN = 1,    /**< Keep minimum sample */
    PCM_SIG_ACCUM_MAX = 2,    /**< Keep maximum sample */
    PCM_SIG_ACCUM_LAST = 3,   /**< Keep only the last sample */
    PCM_SIG_ACCUM_UNSPEC = 4, /**< Accumulator is not specified  */
} pcm_signal_accum_t;

typedef enum signal_trigger {
    PCM_SIG_TRIGGER_RAW = 0,   /**< Signal uses raw signal value */
    PCM_SIG_TRIGGER_DELTA = 1, /**< Signal uses (prev_trigger_val - curr_val)
                                  for trigger evaluation */
    PCM_SIG_TRIGGER_MAGNITUDE = 2, /**< Signal uses abs(prev_trigger_val -
                                      curr_val) for trigger evaluation */
    PCM_SIG_TRIGGER_UNSPEC = 3,    /**< Signal has no trigger */
} pcm_signal_trigger_t;

/**
 * @enum pcm_control_t
 * @brief Identifiers for PCM control knobs.
 */
typedef enum control {
    PCM_CTRL_CWND = 0,    /**< Sending congestion window */
    PCM_CTRL_RATE = 1,    /**< Sending rate control */
    PCM_CTRL_EV = 2,      /**< New entropy value */
    PCM_CTRL_UNKNOWN = 3, /**< Control unknown - used for error handling */
} pcm_control_t;

#ifdef __cplusplus
}
#endif

#endif /* _PCM_H_ */