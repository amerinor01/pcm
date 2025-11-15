#ifndef _PCMH_H_
#define _PCMH_H_

#include "pcm.h"

STATIC_ASSERT(sizeof(pcm_uint) <= sizeof(pcm_int),
              "sizeof(pcm_uint) must be equal to sizeof(pcm_int)");
STATIC_ASSERT(sizeof(pcm_uint) <= sizeof(pcm_float),
              "sizeof(pcm_uint) must be equal to sizeof(pcm_float)");

union pcm_var_converter {
    pcm_uint u;
    pcm_int i;
    pcm_float f;
};

// number of signals is limited by the trigger_mask type width
#define ALGO_CONF_MAX_NUM_SIGNALS (sizeof(pcm_uint))
typedef pcm_uint *pcm_handler_datapath_snapshot;

#define ALGO_CTX_ARGS pcm_handler_datapath_snapshot snapshot
#define ALGO_CTX_PASS snapshot
#define __algorithm_entry_point __algorithm_main(ALGO_CTX_ARGS)
#define __algorithm_entry_point_symbol "__algorithm_main"
typedef pcm_err_t (*pcm_handler_main_cb)(pcm_handler_datapath_snapshot);

/**
 * @brief Algorithm handler entry point
 */
#define algorithm_main() __algorithm_entry_point

static PCM_FORCE_INLINE pcm_uint encode_pcm_int(pcm_int val) {
    union pcm_var_converter converter;
    converter.i = val;
    return converter.u;
}

static PCM_FORCE_INLINE pcm_int decode_pcm_int(pcm_uint val) {
    union pcm_var_converter converter;
    converter.u = val;
    return converter.i;
}

static PCM_FORCE_INLINE pcm_uint encode_pcm_float(pcm_float val) {
    union pcm_var_converter converter;
    converter.f = val;
    return converter.u;
}

static PCM_FORCE_INLINE pcm_float decode_pcm_float(pcm_uint val) {
    union pcm_var_converter converter;
    converter.u = val;
    return converter.f;
}

#ifndef __cplusplus // handler-side API is compiled with C compiler

static PCM_FORCE_INLINE pcm_uint __vm_signal_trigger_mask_get(
    pcm_handler_datapath_snapshot snapshot, size_t mask_offset) {
    return snapshot[mask_offset];
}

static PCM_FORCE_INLINE pcm_uint
__vm_signal_get(const pcm_handler_datapath_snapshot snapshot,
                size_t signal_offset, size_t idx) {
    return snapshot[signal_offset + UTIL_MASK_TO_ARR_IDX(idx)];
}

static PCM_FORCE_INLINE void
__vm_signal_set(pcm_handler_datapath_snapshot snapshot, size_t signal_offset,
                size_t idx, pcm_uint val) {
    snapshot[signal_offset + UTIL_MASK_TO_ARR_IDX(idx)] = val;
}

static PCM_FORCE_INLINE void
__vm_signal_update(pcm_handler_datapath_snapshot snapshot, size_t signal_offset,
                   size_t idx, pcm_uint val) {
    snapshot[signal_offset + UTIL_MASK_TO_ARR_IDX(idx)] += val;
}

static PCM_FORCE_INLINE pcm_uint
__vm_control_get(const pcm_handler_datapath_snapshot snapshot,
                 size_t ctrl_offset, size_t idx) {
    return snapshot[ctrl_offset + idx];
}

static PCM_FORCE_INLINE void
__vm_control_set(pcm_handler_datapath_snapshot snapshot, size_t ctrl_offset,
                 size_t idx, pcm_uint val) {
    snapshot[ctrl_offset + idx] = val;
}

static PCM_FORCE_INLINE pcm_uint
__vm_var_uint_get(const pcm_handler_datapath_snapshot snapshot,
                  size_t var_offset, size_t idx) {
    return snapshot[var_offset + idx];
}

static PCM_FORCE_INLINE void
__vm_var_uint_set(pcm_handler_datapath_snapshot snapshot, size_t var_offset,
                  size_t idx, pcm_uint val) {
    snapshot[var_offset + idx] = val;
}

static PCM_FORCE_INLINE pcm_uint
__vm_arr_uint_get(const pcm_handler_datapath_snapshot snapshot,
                  size_t var_offset, size_t arr_id, size_t idx) {
    return snapshot[var_offset + arr_id + idx];
}

static PCM_FORCE_INLINE void
__vm_arr_uint_set(pcm_handler_datapath_snapshot snapshot, size_t var_offset,
                  size_t arr_id, size_t idx, pcm_uint val) {
    snapshot[var_offset + arr_id + idx] = val;
}

static PCM_FORCE_INLINE pcm_int
__vm_var_int_get(const pcm_handler_datapath_snapshot snapshot,
                 size_t var_offset, size_t idx) {
    return decode_pcm_int(snapshot[var_offset + idx]);
}

static PCM_FORCE_INLINE void
__vm_var_int_set(pcm_handler_datapath_snapshot snapshot, size_t var_offset,
                 size_t idx, pcm_int val) {
    snapshot[var_offset + idx] = encode_pcm_int(val);
}

static PCM_FORCE_INLINE pcm_float
__vm_var_float_get(const pcm_handler_datapath_snapshot snapshot,
                   size_t var_offset, size_t idx) {
    return decode_pcm_float(snapshot[var_offset + idx]);
}

static PCM_FORCE_INLINE void
__vm_var_float_set(pcm_handler_datapath_snapshot snapshot, size_t var_offset,
                   size_t idx, pcm_float val) {
    snapshot[var_offset + idx] = encode_pcm_float(val);
}

/**
 * @brief Get the current persistent state within a handler.
 *
 * Similar to get_var_uint.
 *
 * @param[in] idx   User-defined state index.
 * @return Current state value.
 */
#define get_var(idx) __vm_var_uint_get(snapshot, VAR_OFFSET, idx)

/**
 * @brief Update the persistent state within a handler.
 *
 * Similar to set_var_int.
 *
 * @param[in] idx   User-defined state index.
 * @param[in] val          New state value.
 */
#define set_var(idx, val) __vm_var_uint_set(snapshot, VAR_OFFSET, idx, val);

/**
 * @brief Get the integer current persistent state within a handler.
 *
 * @param[in] idx User-defined state index.
 * @return Current state integer value.
 */
#define get_var_int(idx) __vm_var_int_get(snapshot, VAR_OFFSET, idx)

/**
 * @brief Update the integer persistent state within a handler.
 *
 * @param[in] idx   User-defined state index.
 * @param[in] val          New integer state value.
 */
#define set_var_int(idx, val) __vm_var_int_set(snapshot, VAR_OFFSET, idx, val);

/**
 * @brief Get the unsigned integer current persistent state within a
 * handler.
 *
 * @param[in] idx User-defined state index.
 * @return Current state unsigned integer value.
 */
#define get_var_uint(idx) __vm_var_uint_get(snapshot, VAR_OFFSET, idx)

/**
 * @brief Update the unsigned integer persistent state within a handler.
 *
 * @param[in] idx   User-defined state index.
 * @param[in] val          New unsigned integer state value.
 */
#define set_var_uint(idx, val)                                                 \
    __vm_var_uint_set(snapshot, VAR_OFFSET, idx, val);

/**
 * @brief Get the float current persistent state within a handler.
 *
 * @param[in] idx User-defined state index.
 * @return Current state float value.
 */
#define get_var_float(idx) __vm_var_float_get(snapshot, VAR_OFFSET, idx)

/**
 * @brief Update the float persistent state within a handler.
 *
 * @param[in] idx   User-defined state index.
 * @param[in] val          New float state value.
 */
#define set_var_float(idx, val)                                                \
    __vm_var_float_set(snapshot, VAR_OFFSET, idx, val);

/**
 * @brief Get the unsigned integer current persistent array state within a
 * handler.
 *
 * @param[in] arr_id   User-defined state index.
 * @param[in] idx     Index inside array
 * @return Current state unsigned integer value.
 */
#define get_arr_uint(arr_id, idx)                                              \
    __vm_arr_uint_get(snapshot, VAR_OFFSET, arr_id, idx)

/**
 * @brief Update the unsigned integer persistent array state within a handler.
 *
 * @param[in] arr_id   User-defined state index.
 * @param[in] idx     Index inside array
 * @param[in] val          New unsigned integer state value.
 */
#define set_arr_uint(arr_id, idx, val)                                         \
    __vm_arr_uint_set(snapshot, VAR_OFFSET, arr_id, idx, val);

/**
 * @brief Read the latest signal value within a handler.
 *
 * @param[in] idx   User-defined signal index.
 * @return Current signal value.
 */
#define get_signal(idx) __vm_signal_get(snapshot, SIGNAL_OFFSET, idx)

/**
 * @brief Set the signal value within a handler.
 *
 * @param[in] idx   User-defined signal index.
 * @param[in] val          New signal value.
 */
#define set_signal(idx, val) __vm_signal_set(snapshot, SIGNAL_OFFSET, idx, val)

/**
 * @brief Update the signal value within a handler.
 *
 * @param[in] idx   User-defined signal index.
 * @param[in] val          Update value.
 */
#define update_signal(idx, val)                                                \
    __vm_signal_update(snapshot, SIGNAL_OFFSET, idx, val)

/**
 * @brief Get mask of signals that triggered handler.
 *
 * @return[in] trigger_mask with bits set for signals that triggered handler.
 */
#define get_signal_trigger_mask()                                              \
    __vm_signal_trigger_mask_get(snapshot, MASK_OFFSET)

/**
 * @brief Read the current control knob value within a handler.
 *
 * @param[in] idx   User-defined control index.
 * @return Current control value.
 */
#define get_control(idx) __vm_control_get(snapshot, CONTROL_OFFSET, idx)

/**
 * @brief Update the control knob value within a handler.
 *
 * @param[in] idx   User-defined control index.
 * @param[in] val          New control value.
 */
#define set_control(idx, val) __vm_control_set(snapshot, CONTROL_OFFSET, idx, val)

#endif

#endif /* _PCMH_H_ */
