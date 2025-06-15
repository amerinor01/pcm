#include "impl.h"
#include "util.h"

inline int __flow_signal_get(const void *ctx, size_t user_index) {
    return atomic_load(
        &(((struct flow *)ctx)
              ->datapath_state)[FLOW_SIGNALS_OFFSET + user_index]);
}

inline void __flow_signal_set(void *ctx, size_t user_index, int val) {
    atomic_store(&(((struct flow *)ctx)
                       ->datapath_state)[FLOW_SIGNALS_OFFSET + user_index],
                 val);
}

size_t __flow_signal_trigger_user_index_get(void *ctx) {
    return ((struct flow *)ctx)->trigger_user_index;
}

inline int __flow_control_get(const void *ctx, size_t user_index) {
    return atomic_load(
        &(((struct flow *)ctx)
              ->datapath_state)[FLOW_CONTROLS_OFFSET + user_index]);
}

inline void __flow_control_set(void *ctx, size_t user_index, int val) {
    atomic_store(&(((struct flow *)ctx)
                       ->datapath_state)[FLOW_CONTROLS_OFFSET + user_index],
                 val);
}

inline int __flow_local_state_int_get(const void *ctx, size_t user_index) {
    return decode_int(
        (((struct flow *)ctx)
             ->local_state)[FLOW_LOCAL_STATE_VARS_OFFSET + user_index]);
}

inline void __flow_local_state_int_set(void *ctx, size_t user_index, int val) {
    (((struct flow *)ctx)
         ->local_state)[FLOW_LOCAL_STATE_VARS_OFFSET + user_index] =
        encode_int(val);
}

inline float __flow_local_state_float_get(const void *ctx, size_t user_index) {
    return decode_float(
        (((struct flow *)ctx)
             ->local_state)[FLOW_LOCAL_STATE_VARS_OFFSET + user_index]);
}

inline void __flow_local_state_float_set(void *ctx, size_t user_index,
                                         float val) {
    (((struct flow *)ctx)
         ->local_state)[FLOW_LOCAL_STATE_VARS_OFFSET + user_index] =
        encode_float(val);
}