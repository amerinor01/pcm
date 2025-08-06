#ifndef _PCM_ALGO_H_
#define _PCM_ALGO_H_

#include "pcm.h"

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

#define UTIL_MASK_TO_ARR_IDX(mask) ((unsigned long)__builtin_ctz(mask))

static PCM_FORCE_INLINE pcm_uint encode_pcm_int(pcm_int val) {
    union {
        pcm_int i;
        pcm_uint u;
    } converter;
    converter.i = val;
    return converter.u;
}

static PCM_FORCE_INLINE pcm_int decode_pcm_int(pcm_uint val) {
    union {
        pcm_int i;
        pcm_uint u;
    } converter;
    converter.u = val;
    return converter.i;
}

static PCM_FORCE_INLINE pcm_uint encode_pcm_float(pcm_float val) {
    union {
        pcm_float f;
        pcm_uint u;
    } converter;
    converter.f = val;
    return converter.u;
}

static PCM_FORCE_INLINE pcm_float decode_pcm_float(pcm_uint val) {
    union {
        pcm_float f;
        pcm_uint u;
    } converter;
    converter.u = val;
    return converter.f;
}

static PCM_FORCE_INLINE pcm_uint __flow_signal_trigger_mask_get(void *ctx) {
    return ((struct flow_datapath_snapshot *)ctx)->trigger_mask;
}

static PCM_FORCE_INLINE pcm_uint __flow_signal_get(const void *ctx,
                                                   size_t idx) {
    return ((struct flow_datapath_snapshot *)ctx)
        ->signals[UTIL_MASK_TO_ARR_IDX(idx)];
}

static PCM_FORCE_INLINE void __flow_signal_set(void *ctx, size_t idx,
                                               pcm_uint val) {
    ((struct flow_datapath_snapshot *)ctx)->signals[UTIL_MASK_TO_ARR_IDX(idx)] =
        val;
}

static PCM_FORCE_INLINE void __flow_signal_update(void *ctx, size_t idx,
                                                  pcm_uint val) {
    ((struct flow_datapath_snapshot *)ctx)
        ->signals[UTIL_MASK_TO_ARR_IDX(idx)] += val;
}

static PCM_FORCE_INLINE pcm_uint __flow_control_get(const void *ctx,
                                                    size_t idx) {
    return ((struct flow_datapath_snapshot *)ctx)->controls[idx];
}

static PCM_FORCE_INLINE void __flow_control_set(void *ctx, size_t idx,
                                                pcm_uint val) {
    ((struct flow_datapath_snapshot *)ctx)->controls[idx] = val;
}

static PCM_FORCE_INLINE pcm_uint __flow_var_uint_get(const void *ctx,
                                                     size_t idx) {
    return ((struct flow_datapath_snapshot *)ctx)->vars[idx];
}

static PCM_FORCE_INLINE void __flow_var_uint_set(void *ctx, size_t idx,
                                                 pcm_uint val) {
    ((struct flow_datapath_snapshot *)ctx)->vars[idx] = val;
}

static PCM_FORCE_INLINE pcm_int __flow_var_int_get(const void *ctx,
                                                   size_t idx) {
    return decode_pcm_int(((struct flow_datapath_snapshot *)ctx)->vars[idx]);
}

static PCM_FORCE_INLINE void __flow_var_int_set(void *ctx, size_t idx,
                                                pcm_int val) {
    ((struct flow_datapath_snapshot *)ctx)->vars[idx] = encode_pcm_int(val);
}

static PCM_FORCE_INLINE pcm_float __flow_var_float_get(const void *ctx,
                                                       size_t idx) {
    return decode_pcm_float(((struct flow_datapath_snapshot *)ctx)->vars[idx]);
}

static PCM_FORCE_INLINE void __flow_var_float_set(void *ctx, size_t idx,
                                                  pcm_float val) {
    ((struct flow_datapath_snapshot *)ctx)->vars[idx] = encode_pcm_float(val);
}

#endif /* _PCM_ALGO_H_ */
