#include "impl.h"

int __flow_signal_get(const void *ctx, size_t user_index) {
    return atomic_load(
        &(((struct flow *)ctx)
              ->datapath_state)[FLOW_SIGNALS_OFFSET + user_index]);
}

void __flow_signal_set(void *ctx, size_t user_index, int val) {
    atomic_store(&(((struct flow *)ctx)
                       ->datapath_state)[FLOW_SIGNALS_OFFSET + user_index],
                 val);
}

inline int __flow_control_get(const void *ctx, size_t user_index) {
    return atomic_load(
        &(((struct flow *)ctx)
              ->datapath_state)[FLOW_CONTROLS_OFFSET + user_index]);
}

void __flow_control_set(void *ctx, size_t user_index, int val) {
    atomic_store(&(((struct flow *)ctx)
                       ->datapath_state)[FLOW_CONTROLS_OFFSET + user_index],
                 val);
}

int __flow_local_state_get(const void *ctx, size_t user_index) {
    return (((struct flow *)ctx)
                ->local_state)[FLOW_LOCAL_STATE_VARS_OFFSET + user_index];
}

void __flow_local_state_set(void *ctx, size_t user_index, int val) {
    (((struct flow *)ctx)
         ->local_state)[FLOW_LOCAL_STATE_VARS_OFFSET + user_index] = val;
}