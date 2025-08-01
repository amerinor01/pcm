#include "impl.h"
#include "util.h"

inline pcm_uint __flow_signal_trigger_mask_get(void *ctx) {
    return ((struct flow *)ctx)->trigger_mask;
}

/*
 * Note: functions below are not optimized for performance as
 * they incure at least one function call and some pointer chasing.
 *
 * TODO: Calling backend-specific logic without overhead should be possible by
 * supporting preprocessing/compiling.
 */

inline pcm_uint __flow_signal_get(const void *ctx, size_t idx) {
    return ((struct flow *)ctx)->device->flow_ops.handler.signal_get(ctx, idx);
}

inline void __flow_signal_set(void *ctx, size_t idx, pcm_uint val) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.signal_set(ctx, idx, val);
}

inline void __flow_signal_update(void *ctx, size_t idx, pcm_uint val) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.signal_update(ctx, idx, val);
}

inline pcm_uint __flow_control_get(const void *ctx, size_t idx) {
    return ((struct flow *)ctx)->device->flow_ops.handler.control_get(ctx, idx);
}

inline void __flow_control_set(void *ctx, size_t idx, pcm_uint val) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.control_set(ctx, idx, val);
}

inline pcm_int __flow_var_int_get(const void *ctx, size_t idx) {
    return ((struct flow *)ctx)->device->flow_ops.handler.var_int_get(ctx, idx);
}

inline void __flow_var_int_set(void *ctx, size_t idx, pcm_int val) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.var_int_set(ctx, idx, val);
}

inline pcm_uint __flow_var_uint_get(const void *ctx, size_t idx) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.var_uint_get(ctx, idx);
}

inline void __flow_var_uint_set(void *ctx, size_t idx, pcm_uint val) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.var_uint_set(ctx, idx, val);
}

inline pcm_float __flow_var_float_get(const void *ctx, size_t idx) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.var_float_get(ctx, idx);
}

inline void __flow_var_float_set(void *ctx, size_t idx, pcm_float val) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.var_float_set(ctx, idx, val);
}