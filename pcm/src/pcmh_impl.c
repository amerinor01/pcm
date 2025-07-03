#include "impl.h"
#include "util.h"

inline size_t __flow_signal_trigger_user_index_get(void *ctx) {
    return ((struct flow *)ctx)->trigger_user_index;
}

/*
 * Note: functions below are not optimized for performance as
 * they incure at least one function call and some pointer chasing.
 *
 * TODO: Calling backend-specific logic without overhead should be possible by
 * supporting preprocessing/compiling.
 */

inline pcm_uint __flow_signal_get(const void *ctx, size_t user_index) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.signal_get(ctx, user_index);
}

inline void __flow_signal_set(void *ctx, size_t user_index, pcm_uint val) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.signal_set(ctx, user_index, val);
}

inline void __flow_signal_update(void *ctx, size_t user_index, pcm_uint val) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.signal_update(ctx, user_index, val);
}

inline pcm_uint __flow_control_get(const void *ctx, size_t user_index) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.control_get(ctx, user_index);
}

inline void __flow_control_set(void *ctx, size_t user_index, pcm_uint val) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.control_set(ctx, user_index, val);
}

inline pcm_int __flow_local_state_int_get(const void *ctx, size_t user_index) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.local_state_int_get(ctx, user_index);
}

inline void __flow_local_state_int_set(void *ctx, size_t user_index,
                                       pcm_int val) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.local_state_int_set(ctx, user_index, val);
}

inline pcm_uint __flow_local_state_uint_get(const void *ctx,
                                            size_t user_index) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.local_state_uint_get(ctx, user_index);
}

inline void __flow_local_state_uint_set(void *ctx, size_t user_index,
                                        pcm_uint val) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.local_state_uint_set(ctx, user_index, val);
}

inline pcm_float __flow_local_state_float_get(const void *ctx,
                                              size_t user_index) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.local_state_float_get(ctx, user_index);
}

inline void __flow_local_state_float_set(void *ctx, size_t user_index,
                                         pcm_float val) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.local_state_float_set(ctx, user_index, val);
}

inline pcm_int __flow_constant_int_get(const void *ctx, size_t user_index) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.constant_int_get(ctx, user_index);
}

inline pcm_uint __flow_constant_uint_get(const void *ctx, size_t user_index) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.constant_uint_get(ctx, user_index);
}

inline pcm_float __flow_constant_float_get(const void *ctx, size_t user_index) {
    return ((struct flow *)ctx)
        ->device->flow_ops.handler.constant_float_get(ctx, user_index);
}