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
#define ALGO_CONF_MAX_NUM_SIGNALS (sizeof(pcm_uint) * 8)
#define ALGO_CONF_MAX_NUM_CONTROLS 2
#define ALGO_CONF_MAX_VARS 16

struct flow_datapath_snapshot {
    pcm_uint trigger_mask;
    pcm_uint signals[ALGO_CONF_MAX_NUM_SIGNALS];
    pcm_uint thresholds[ALGO_CONF_MAX_NUM_SIGNALS];
    pcm_uint controls[ALGO_CONF_MAX_NUM_CONTROLS];
    pcm_uint vars[ALGO_CONF_MAX_VARS];
};

#define ALGO_CTX_ARGS struct flow_datapath_snapshot *snapshot
#define ALGO_CTX_PASS snapshot
#define __algorithm_entry_point __algorithm_main(ALGO_CTX_ARGS)
#define __algorithm_entry_point_symbol "__algorithm_main"

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

static PCM_FORCE_INLINE pcm_uint
__flow_signal_trigger_mask_get(struct flow_datapath_snapshot *snapshot) {
    return snapshot->trigger_mask;
}

static PCM_FORCE_INLINE pcm_uint
__flow_signal_get(const struct flow_datapath_snapshot *snapshot, size_t idx) {
    return snapshot->signals[UTIL_MASK_TO_ARR_IDX(idx)];
}

static PCM_FORCE_INLINE void
__flow_signal_set(struct flow_datapath_snapshot *snapshot, size_t idx,
                  pcm_uint val) {
    snapshot->signals[UTIL_MASK_TO_ARR_IDX(idx)] = val;
}

static PCM_FORCE_INLINE void
__flow_signal_update(struct flow_datapath_snapshot *snapshot, size_t idx,
                     pcm_uint val) {
    snapshot->signals[UTIL_MASK_TO_ARR_IDX(idx)] += val;
}

static PCM_FORCE_INLINE pcm_uint
__flow_control_get(const struct flow_datapath_snapshot *snapshot, size_t idx) {
    return snapshot->controls[idx];
}

static PCM_FORCE_INLINE void
__flow_control_set(struct flow_datapath_snapshot *snapshot, size_t idx,
                   pcm_uint val) {
    snapshot->controls[idx] = val;
}

static PCM_FORCE_INLINE pcm_uint
__flow_var_uint_get(const struct flow_datapath_snapshot *snapshot, size_t idx) {
    return snapshot->vars[idx];
}

static PCM_FORCE_INLINE void
__flow_var_uint_set(struct flow_datapath_snapshot *snapshot, size_t idx,
                    pcm_uint val) {
    snapshot->vars[idx] = val;
}

static PCM_FORCE_INLINE pcm_int
__flow_var_int_get(const struct flow_datapath_snapshot *snapshot, size_t idx) {
    return decode_pcm_int(snapshot->vars[idx]);
}

static PCM_FORCE_INLINE void
__flow_var_int_set(struct flow_datapath_snapshot *snapshot, size_t idx,
                   pcm_int val) {
    snapshot->vars[idx] = encode_pcm_int(val);
}

static PCM_FORCE_INLINE pcm_float __flow_var_float_get(
    const struct flow_datapath_snapshot *snapshot, size_t idx) {
    return decode_pcm_float(snapshot->vars[idx]);
}

static PCM_FORCE_INLINE void
__flow_var_float_set(struct flow_datapath_snapshot *snapshot, size_t idx,
                     pcm_float val) {
    snapshot->vars[idx] = encode_pcm_float(val);
}

/**
 * @brief Get the current persistent state within a handler.
 *
 * Similar to get_var_uint.
 *
 * @param[in] idx   User-defined state index.
 * @return Current state value.
 */
#define get_var(idx) __flow_var_uint_get(snapshot, idx)

/**
 * @brief Update the persistent state within a handler.
 *
 * Similar to set_var_int.
 *
 * @param[in] idx   User-defined state index.
 * @param[in] val          New state value.
 */
#define set_var(idx, val) __flow_var_uint_set(snapshot, idx, val);

/**
 * @brief Get the integer current persistent state within a handler.
 *
 * @param[in] idx User-defined state index.
 * @return Current state integer value.
 */
#define get_var_int(idx) __flow_var_int_get(snapshot, idx)

/**
 * @brief Update the integer persistent state within a handler.
 *
 * @param[in] idx   User-defined state index.
 * @param[in] val          New integer state value.
 */
#define set_var_int(idx, val) __flow_var_int_set(snapshot, idx, val);

/**
 * @brief Get the unsigned integer current persistent state within a
 * handler.
 *
 * @param[in] idx User-defined state index.
 * @return Current state unsigned integer value.
 */
#define get_var_uint(idx) __flow_var_uint_get(snapshot, idx)

/**
 * @brief Update the unsigned integer persistent state within a handler.
 *
 * @param[in] idx   User-defined state index.
 * @param[in] val          New unsigned integer state value.
 */
#define set_var_uint(idx, val) __flow_var_uint_set(snapshot, idx, val);

/**
 * @brief Get the float current persistent state within a handler.
 *
 * @param[in] idx User-defined state index.
 * @return Current state float value.
 */
#define get_var_float(idx) __flow_var_float_get(snapshot, idx)

/**
 * @brief Update the float persistent state within a handler.
 *
 * @param[in] idx   User-defined state index.
 * @param[in] val          New float state value.
 */
#define set_var_float(idx, val) __flow_var_float_set(snapshot, idx, val);

/**
 * @brief Read the latest signal value within a handler.
 *
 * @param[in] idx   User-defined signal index.
 * @return Current signal value.
 */
#define get_signal(idx) __flow_signal_get(snapshot, idx)

/**
 * @brief Set the signal value within a handler.
 *
 * @param[in] idx   User-defined signal index.
 * @param[in] val          New signal value.
 */
#define set_signal(idx, val) __flow_signal_set(snapshot, idx, val)

#define PCM_SIG_REARM (UINT64_MAX - 1)

/**
 * @brief Update the signal value within a handler.
 *
 * @param[in] idx   User-defined signal index.
 * @param[in] val          Update value.
 */
#define update_signal(idx, val) __flow_signal_update(snapshot, idx, val)

/**
 * @brief Get mask of signals that triggered handler.
 *
 * @return[in] trigger_mask with bits set for signals that triggered handler.
 */
#define get_signal_trigger_mask() __flow_signal_trigger_mask_get(snapshot)

/**
 * @brief Read the current control knob value within a handler.
 *
 * @param[in] idx   User-defined control index.
 * @return Current control value.
 */
#define get_control(idx) __flow_control_get(snapshot, idx)

/**
 * @brief Update the control knob value within a handler.
 *
 * @param[in] idx   User-defined control index.
 * @param[in] val          New control value.
 */
#define set_control(idx, val) __flow_control_set(snapshot, idx, val)

#endif /* _PCMH_H_ */
