#ifndef _UTIL_H_
#define _UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include "impl.h"
#include "pcm_log.h"

#define container_of(ptr, type, member)                                        \
    ({                                                                         \
        const typeof(((type *)0)->member) *__mptr = (ptr);                     \
        (type *)((char *)__mptr - offsetof(type, member));                     \
    })

#define ATTR_LIST_ITEM_ALLOC(attr_list, user_index, item_counter, max_items,   \
                             attr_ptr)                                         \
    {                                                                          \
        if ((item_counter) >= (max_items)) {                                   \
            PCM_LOG_CRIT("[attr_list=%p] %s list storage is full", attr_list,  \
                         #attr_list);                                          \
            return PCM_ERROR;                                                  \
        }                                                                      \
        if ((user_index) >= (max_items)) {                                     \
            PCM_LOG_CRIT("[attr_list=%p] user_index exeeds %s list capacity",  \
                         attr_list, #attr_list);                               \
            return PCM_ERROR;                                                  \
        }                                                                      \
        (attr_ptr) = calloc(1, sizeof(*(attr_ptr)));                           \
        if (!(attr_ptr)) {                                                     \
            PCM_LOG_CRIT("[attr_list=%p] failed to allocate new attribute",    \
                         attr_list);                                           \
            return PCM_ERROR;                                                  \
        }                                                                      \
        (attr_ptr)->metadata.index = (user_index);                             \
        slist_insert_head(&(attr_ptr)->metadata.list_entry, (attr_list));      \
        ++(item_counter);                                                      \
    }

#define ATTR_LIST_FREE(attr_list, attr_type, item_counter)                     \
    if (item_counter) {                                                        \
        while (!slist_empty(attr_list)) {                                      \
            struct slist_entry *entry = slist_remove_head(attr_list);          \
            attr_type *attr =                                                  \
                container_of(entry, attr_type, metadata.list_entry);           \
            free(attr);                                                        \
            --(item_counter);                                                  \
        }                                                                      \
    }

#define ATTR_LIST_DUPLICATE_USER_INDEX_CHK(attr_list, attr_type, user_index)   \
    {                                                                          \
        struct slist_entry *item, *prev;                                       \
        slist_foreach(attr_list, item, prev) {                                 \
            (void)prev; /* suppress complier warning */                        \
            if (container_of(item, attr_type, metadata.list_entry)             \
                    ->metadata.index == (user_index)) {                        \
                PCM_LOG_CRIT("[attr_list=%p] found duplicate index=%zu",       \
                             attr_list, user_index);                           \
                return PCM_ERROR;                                              \
            }                                                                  \
        }                                                                      \
    }

#define ATTR_LIST_DUPLICATE_TYPE_CHK(attr_list, attr_type, chk_type)           \
    {                                                                          \
        struct slist_entry *item, *prev;                                       \
        slist_foreach(attr_list, item, prev) {                                 \
            (void)prev; /* suppress complier warning */                        \
            if (container_of(item, attr_type, metadata.list_entry)->type ==    \
                (chk_type)) {                                                  \
                PCM_LOG_CRIT("[attr_list=%p] found duplicate type=%d",         \
                             attr_list, chk_type);                             \
                return PCM_ERROR;                                              \
            }                                                                  \
        }                                                                      \
    }

#define ATTR_LIST_FIRST_MATCH_BY_ATTR_TYPE_FIND(attr_list, attr_type,          \
                                                entry_type, found_idx)         \
    {                                                                          \
        struct slist_entry *item, *prev;                                       \
        (found_idx) = SIZE_MAX;                                                \
        slist_foreach(attr_list, item, prev) {                                 \
            (void)prev; /* suppress complier warning */                        \
            attr_type *attr =                                                  \
                container_of(item, attr_type, metadata.list_entry);            \
            if (attr->type == (entry_type)) {                                  \
                (found_idx) = attr->metadata.index;                            \
                break;                                                         \
            }                                                                  \
        }                                                                      \
    }

#define ATTR_LIST_ITEM_SET(attr_list, attr_type, user_index, val,              \
                           found_attr_ptr)                                     \
    {                                                                          \
        if (slist_empty(attr_list)) {                                          \
            PCM_LOG_CRIT(                                                      \
                "[attr_list=%p] item set on an empty list at index=%zu",       \
                attr_list, user_index);                                        \
            return PCM_ERROR;                                                  \
        }                                                                      \
        (found_attr_ptr) = NULL;                                               \
        attr_type *cur_attr = NULL;                                            \
        struct slist_entry *item, *prev;                                       \
        slist_foreach(attr_list, item, prev) {                                 \
            (void)prev; /* suppress complier warning */                        \
            cur_attr = container_of(item, attr_type, metadata.list_entry);     \
            if (cur_attr->metadata.index == (user_index)) {                    \
                (found_attr_ptr) = cur_attr;                                   \
                break;                                                         \
            }                                                                  \
        }                                                                      \
        if (!(found_attr_ptr)) {                                               \
            PCM_LOG_CRIT(                                                      \
                "[attr_list=%p] failed to find attribute with index=%zu",      \
                attr_list, user_index);                                        \
            return PCM_ERROR;                                                  \
        }                                                                      \
        (found_attr_ptr)->metadata.value = val;                                \
    }

#define ATTR_LIST_FLOW_STATE_INIT(attr_list, attr_type, state_ptr)             \
    {                                                                          \
        attr_type *cur_attr = NULL;                                            \
        struct slist_entry *item, *prev;                                       \
        slist_foreach(attr_list, item, prev) {                                 \
            (void)prev; /* suppress complier warning */                        \
            cur_attr = container_of(item, attr_type, metadata.list_entry);     \
            (state_ptr)[cur_attr->metadata.index] = cur_attr->metadata.value;  \
        }                                                                      \
    }

static inline const char *signal_type_to_string(pcm_signal_t type) {
    switch (type) {
    case PCM_SIG_ACK:
        return "PCM_SIG_ACK";
    case PCM_SIG_RTO:
        return "PCM_SIG_RTO";
    case PCM_SIG_NACK:
        return "PCM_SIG_NACK";
    case PCM_SIG_ECN:
        return "PCM_SIG_ECN";
    case PCM_SIG_RTT:
        return "PCM_SIG_RTT";
    case PCM_SIG_IN_FLIGHT:
        return "PCM_SIG_IN_FLIGHT";
    case PCM_SIG_DATA_NACKED:
        return "PCM_SIG_DATA_NACKED";
    case PCM_SIG_DATA_TX:
        return "PCM_SIG_DATA_TX";
    case PCM_SIG_ELAPSED_TIME:
        return "PCM_SIG_ELAPSED_TIME";
    default:
        return "SIG_UNKNOWN";
    }
    return "SIG_ACCUM_UNKNOWN";
}

static inline const char *signal_accum_type_to_string(pcm_signal_accum_t type) {
    switch (type) {
    case PCM_SIG_ACCUM_SUM:
        return "SIG_ACCUM_SUM";
    case PCM_SIG_ACCUM_MIN:
        return "PCM_SIG_ACCUM_MIN";
    case PCM_SIG_ACCUM_MAX:
        return "PCM_SIG_ACCUM_MAX";
    case PCM_SIG_ACCUM_LAST:
        return "PCM_SIG_ACCUM_LAST";
    default:
        return "SIG_ACCUM_UNKNOWN";
    }
    return "SIG_ACCUM_UNKNOWN";
}

/* encode any value T into pcm_uint representation */
static inline pcm_uint encode_to_pcm_uint(const void *val, size_t val_size) {
    pcm_uint u;
    memcpy(&u, val, val_size);
    return u;
}

/* decode a pcm_uint raw into a value T */
static inline void decode_from_pcm_uint(pcm_uint u, void *out,
                                        size_t out_size) {
    memcpy(out, &u, out_size);
}

static inline pcm_float decode_pcm_float(pcm_uint u) {
    pcm_float f;
    decode_from_pcm_uint(u, &f, sizeof(f));
    return f;
}

static inline pcm_int decode_pcm_int(pcm_uint u) {
    pcm_int x;
    decode_from_pcm_uint(u, &x, sizeof(x));
    return x;
}

static inline pcm_uint encode_pcm_float(pcm_float f) {
    return encode_to_pcm_uint(&f, sizeof(f));
}

static inline pcm_uint encode_pcm_int(pcm_int x) {
    return encode_to_pcm_uint(&x, sizeof(x));
}

static inline struct timespec clock_gettime_now() {
    struct timespec now_ts;
    if (clock_gettime(CLOCK_MONOTONIC, &now_ts)) {
        PCM_LOG_FATAL("clock_gettime failed");
    }
    return now_ts;
}

static inline pcm_uint clock_gettime_ts_diff_us_get(struct timespec ts_start,
                                                    struct timespec ts_end) {
    // TODO: use UEC uses 128 ns as a unit of time
    return (
        pcm_uint)((((double)ts_end.tv_sec * 1e9 + (double)ts_end.tv_nsec) -
                   ((double)ts_start.tv_sec * 1e9 + (double)ts_start.tv_nsec)) /
                  1e3); // to microseconds
}

static inline pcm_uint picosec_ts_diff_us_get(uint64_t ts_start,
                                              uint64_t ts_end) {
    // return (pcm_uint)((double)(ts_end - ts_start) / 1000000.0);
    return (pcm_uint)(ts_end - ts_start);
}

#define PLUGIN_FLOW_SIGNAL_GET_GENERIC_FN(plugin_name)                         \
    plugin_name##_flow_signal_get
#define PLUGIN_FLOW_SIGNAL_GET_GENERIC_DEFINE(plugin_name)                     \
    static inline pcm_uint PLUGIN_FLOW_SIGNAL_GET_GENERIC_FN(plugin_name)(     \
        const void *ctx, size_t user_index) {                                  \
        struct plugin_name##_flow *flow_ctx =                                  \
            ((struct plugin_name##_flow *)(((pcm_flow_t)ctx)->backend_ctx));   \
        return flow_ctx->signals[user_index];                                  \
    }

#define PLUGIN_FLOW_SIGNAL_SET_GENERIC_FN(plugin_name)                         \
    plugin_name##_flow_signal_set
#define PLUGIN_FLOW_SIGNAL_SET_GENERIC_DEFINE(plugin_name)                     \
    static inline void PLUGIN_FLOW_SIGNAL_SET_GENERIC_FN(plugin_name)(         \
        void *ctx, size_t user_index, pcm_uint val) {                          \
        struct plugin_name##_flow *flow_ctx =                                  \
            ((struct plugin_name##_flow *)(((pcm_flow_t)ctx)->backend_ctx));   \
        flow_ctx->signals[user_index] = val;                                   \
    }

#define PLUGIN_FLOW_SIGNAL_UPDATE_GENERIC_FN(plugin_name)                      \
    plugin_name##_flow_signal_update
#define PLUGIN_FLOW_SIGNAL_UPDATE_GENERIC_DEFINE(plugin_name)                  \
    static inline void PLUGIN_FLOW_SIGNAL_UPDATE_GENERIC_FN(plugin_name)(      \
        void *ctx, size_t user_index, pcm_uint val) {                          \
        struct plugin_name##_flow *flow_ctx =                                  \
            ((struct plugin_name##_flow *)(((pcm_flow_t)ctx)->backend_ctx));   \
        flow_ctx->signals[user_index] += val;                                  \
    }

#define PLUGIN_FLOW_CONTROL_GET_GENERIC_FN(plugin_name)                        \
    plugin_name##_flow_control_get
#define PLUGIN_FLOW_CONTROL_GET_GENERIC_DEFINE(plugin_name)                    \
    static inline pcm_uint PLUGIN_FLOW_CONTROL_GET_GENERIC_FN(plugin_name)(    \
        const void *ctx, size_t user_index) {                                  \
        struct plugin_name##_flow *flow_ctx =                                  \
            ((struct plugin_name##_flow *)(((pcm_flow_t)ctx)->backend_ctx));   \
        return flow_ctx->controls[user_index];                                 \
    }

#define PLUGIN_FLOW_CONTROL_SET_GENERIC_FN(plugin_name)                        \
    plugin_name##_flow_control_set
#define PLUGIN_FLOW_CONTROL_SET_GENERIC_DEFINE(plugin_name)                    \
    static inline void PLUGIN_FLOW_CONTROL_SET_GENERIC_FN(plugin_name)(        \
        void *ctx, size_t user_index, pcm_uint val) {                          \
        struct plugin_name##_flow *flow_ctx =                                  \
            ((struct plugin_name##_flow *)(((pcm_flow_t)ctx)->backend_ctx));   \
        flow_ctx->controls[user_index] = val;                                  \
    }

#define PLUGIN_FLOW_VAR_UINT_GET_GENERIC_FN(plugin_name)                       \
    plugin_name##_flow_VAR_uint_get
#define PLUGIN_FLOW_VAR_UINT_GET_GENERIC_DEFINE(plugin_name)                   \
    static inline pcm_uint PLUGIN_FLOW_VAR_UINT_GET_GENERIC_FN(plugin_name)(   \
        const void *ctx, size_t user_index) {                                  \
        struct plugin_name##_flow *flow_ctx =                                  \
            ((struct plugin_name##_flow *)(((pcm_flow_t)ctx)->backend_ctx));   \
        return flow_ctx->vars[user_index];                                     \
    }

#define PLUGIN_FLOW_VAR_UINT_SET_GENERIC_FN(plugin_name)                       \
    plugin_name##_flow_VAR_uint_set
#define PLUGIN_FLOW_VAR_UINT_SET_GENERIC_DEFINE(plugin_name)                   \
    static inline void PLUGIN_FLOW_VAR_UINT_SET_GENERIC_FN(plugin_name)(       \
        void *ctx, size_t user_index, pcm_uint val) {                          \
        struct plugin_name##_flow *flow_ctx =                                  \
            ((struct plugin_name##_flow *)(((pcm_flow_t)ctx)->backend_ctx));   \
        flow_ctx->vars[user_index] = val;                                      \
    }

#define PLUGIN_FLOW_VAR_INT_GET_GENERIC_FN(plugin_name)                        \
    plugin_name##_flow_VAR_int_get
#define PLUGIN_FLOW_VAR_INT_GET_GENERIC_DEFINE(plugin_name)                    \
    static inline pcm_int PLUGIN_FLOW_VAR_INT_GET_GENERIC_FN(plugin_name)(     \
        const void *ctx, size_t user_index) {                                  \
        struct plugin_name##_flow *flow_ctx =                                  \
            ((struct plugin_name##_flow *)(((pcm_flow_t)ctx)->backend_ctx));   \
        return decode_pcm_int(flow_ctx->vars[user_index]);                     \
    }

#define PLUGIN_FLOW_VAR_INT_SET_GENERIC_FN(plugin_name)                        \
    plugin_name##_flow_VAR_int_set
#define PLUGIN_FLOW_VAR_INT_SET_GENERIC_DEFINE(plugin_name)                    \
    static inline void PLUGIN_FLOW_VAR_INT_SET_GENERIC_FN(plugin_name)(        \
        void *ctx, size_t user_index, pcm_int val) {                           \
        struct plugin_name##_flow *flow_ctx =                                  \
            ((struct plugin_name##_flow *)(((pcm_flow_t)ctx)->backend_ctx));   \
        flow_ctx->vars[user_index] = encode_pcm_int(val);                      \
    }

#define PLUGIN_FLOW_VAR_FLOAT_GET_GENERIC_FN(plugin_name)                      \
    plugin_name##_flow_VAR_float_get
#define PLUGIN_FLOW_VAR_FLOAT_GET_GENERIC_DEFINE(plugin_name)                  \
    static inline pcm_float PLUGIN_FLOW_VAR_FLOAT_GET_GENERIC_FN(plugin_name)( \
        const void *ctx, size_t user_index) {                                  \
        struct plugin_name##_flow *flow_ctx =                                  \
            ((struct plugin_name##_flow *)(((pcm_flow_t)ctx)->backend_ctx));   \
        return decode_pcm_float(flow_ctx->vars[user_index]);                   \
    }

#define PLUGIN_FLOW_VAR_FLOAT_SET_GENERIC_FN(plugin_name)                      \
    plugin_name##_flow_VAR_float_set
#define PLUGIN_FLOW_VAR_FLOAT_SET_GENERIC_DEFINE(plugin_name)                  \
    static inline void PLUGIN_FLOW_VAR_FLOAT_SET_GENERIC_FN(plugin_name)(      \
        void *ctx, size_t user_index, pcm_float val) {                         \
        struct plugin_name##_flow *flow_ctx =                                  \
            ((struct plugin_name##_flow *)(((pcm_flow_t)ctx)->backend_ctx));   \
        flow_ctx->vars[user_index] = encode_pcm_float(val);                    \
    }

#define PLUGIN_FLOW_ACCUMULATION_OP_SUM_GENERIC_FN(plugin_name)                \
    plugin_name##_flow_signal_accumulation_op_sum
#define PLUGIN_FLOW_ACCUMULATION_OP_SUM_GENERIC_DEFINE(plugin_name)            \
    static inline void PLUGIN_FLOW_ACCUMULATION_OP_SUM_GENERIC_FN(             \
        plugin_name)(pcm_flow_t flow, const struct signal_attr *attr,          \
                     pcm_uint signal) {                                        \
        struct plugin_name##_flow *flow_ctx =                                  \
            (struct plugin_name##_flow *)(flow->backend_ctx);                  \
        flow_ctx->signals[attr->metadata.index] += signal;                     \
    }

#define PLUGIN_FLOW_ACCUMULATION_OP_LAST_GENERIC_FN(plugin_name)               \
    plugin_name##_flow_signal_accumulation_op_last
#define PLUGIN_FLOW_ACCUMULATION_OP_LAST_GENERIC_DEFINE(plugin_name)           \
    static inline void PLUGIN_FLOW_ACCUMULATION_OP_LAST_GENERIC_FN(            \
        plugin_name)(pcm_flow_t flow, const struct signal_attr *attr,          \
                     pcm_uint signal) {                                        \
        struct plugin_name##_flow *flow_ctx =                                  \
            (struct plugin_name##_flow *)(flow->backend_ctx);                  \
        flow_ctx->signals[attr->metadata.index] = signal;                      \
    }

#define PLUGIN_FLOW_ACCUMULATION_OP_MIN_GENERIC_FN(plugin_name)                \
    plugin_name##_flow_signal_accumulation_op_min
#define PLUGIN_FLOW_ACCUMULATION_OP_MIN_GENERIC_DEFINE(plugin_name)            \
    static inline void PLUGIN_FLOW_ACCUMULATION_OP_MIN_GENERIC_FN(             \
        plugin_name)(pcm_flow_t flow, const struct signal_attr *attr,          \
                     pcm_uint signal) {                                        \
        struct plugin_name##_flow *flow_ctx =                                  \
            (struct plugin_name##_flow *)(flow->backend_ctx);                  \
        if (signal < flow_ctx->signals[attr->metadata.index]) {                \
            flow_ctx->signals[attr->metadata.index] = signal;                  \
        }                                                                      \
    }

#define PLUGIN_FLOW_ACCUMULATION_OP_MAX_GENERIC_FN(plugin_name)                \
    plugin_name##_flow_signal_accumulation_op_max
#define PLUGIN_FLOW_ACCUMULATION_OP_MAX_GENERIC_DEFINE(plugin_name)            \
    static inline void PLUGIN_FLOW_ACCUMULATION_OP_MAX_GENERIC_FN(             \
        plugin_name)(pcm_flow_t flow, const struct signal_attr *attr,          \
                     pcm_uint signal) {                                        \
        struct plugin_name##_flow *flow_ctx =                                  \
            (struct plugin_name##_flow *)(flow->backend_ctx);                  \
        if (signal > flow_ctx->signals[attr->metadata.index]) {                \
            flow_ctx->signals[attr->metadata.index] = signal;                  \
        }                                                                      \
    }

#define PLUGIN_FLOW_TRIGGER_OVERFLOW_CHECK_GENERIC_FN(plugin_name)             \
    plugin_name##_flow_trigger_overflow_check
#define PLUGIN_FLOW_TRIGGER_OVERFLOW_CHECK_GENERIC_DEFINE(plugin_name)         \
    static inline bool PLUGIN_FLOW_TRIGGER_OVERFLOW_CHECK_GENERIC_FN(          \
        plugin_name)(const pcm_flow_t flow, const struct signal_attr *attr) {  \
        struct plugin_name##_flow *flow_ctx =                                  \
            (struct plugin_name##_flow *)(flow->backend_ctx);                  \
        pcm_uint value = flow_ctx->signals[attr->metadata.index];              \
        pcm_uint threshold = flow_ctx->thresholds[attr->metadata.index];       \
        if (value >= threshold)                                                \
            return true;                                                       \
        return false;                                                          \
    }

/* post reset state of 1 indicates that burst is an active trigger */
#define PLUGIN_FLOW_TRIGGER_BURST_RESET_GENERIC_FN(plugin_name)                \
    plugin_name##_flow_trigger_burst_reset
#define PLUGIN_FLOW_TRIGGER_BURST_RESET_GENERIC_DEFINE(plugin_name)            \
    static inline void PLUGIN_FLOW_TRIGGER_BURST_RESET_GENERIC_FN(             \
        plugin_name)(pcm_flow_t flow, const struct signal_attr *attr) {        \
        struct plugin_name##_flow *flow_ctx =                                  \
            (struct plugin_name##_flow *)(flow->backend_ctx);                  \
        pcm_uint burst = flow_ctx->signals[attr->metadata.index];              \
        if (burst) {                                                           \
            flow_ctx->signals[attr->metadata.index] = 1;                       \
        }                                                                      \
    }

#define PLUGIN_FLOW_TRIGGER_BURST_CHECK_GENERIC_FN(plugin_name)                \
    plugin_name##_flow_trigger_burst_check
/* subtract 1 below to remove the original activation flag */
#define PLUGIN_FLOW_TRIGGER_BURST_CHECK_GENERIC_DEFINE(plugin_name)            \
    static inline bool PLUGIN_FLOW_TRIGGER_BURST_CHECK_GENERIC_FN(             \
        plugin_name)(const pcm_flow_t flow, const struct signal_attr *attr) {  \
        struct plugin_name##_flow *flow_ctx =                                  \
            (struct plugin_name##_flow *)(flow->backend_ctx);                  \
        pcm_uint burst = flow_ctx->signals[attr->metadata.index];              \
        pcm_uint threshold = flow_ctx->thresholds[attr->metadata.index];       \
        if (burst) {                                                           \
            if ((burst - 1) >= threshold) {                                    \
                return true;                                                   \
            }                                                                  \
        }                                                                      \
        return false;                                                          \
    }

#define PLUGIN_FLOW_SIGNAL_ELAPSED_TIME_ACCUMULATION_OP_GENERIC_FN(            \
    plugin_name)                                                               \
    plugin_name##_flow_signal_elapsed_time_accumulation_op
#define PLUGIN_FLOW_SIGNAL_ELAPSED_TIME_ACCUMULATION_OP_GENERIC_DEFINE(        \
    plugin_name, time_now_fn, time_diff_fn)                                    \
    static inline void                                                         \
    PLUGIN_FLOW_SIGNAL_ELAPSED_TIME_ACCUMULATION_OP_GENERIC_FN(plugin_name)(   \
        pcm_flow_t flow, const struct signal_attr *attr, pcm_uint signal) {    \
        (void)signal;                                                          \
        struct plugin_name##_flow *flow_ctx =                                  \
            (struct plugin_name##_flow *)(flow->backend_ctx);                  \
        flow_ctx->signals[attr->metadata.index] =                              \
            time_diff_fn(flow_ctx->start_ts, time_now_fn());                   \
    }

#define PLUGIN_FLOW_TRIGGER_TIMER_CHECK_GENERIC_FN(plugin_name)                \
    plugin_name##_flow_trigger_timer_check
#define PLUGIN_FLOW_TRIGGER_TIMER_CHECK_GENERIC_DEFINE(                        \
    plugin_name, time_now_fn, time_diff_fn)                                    \
    static inline bool PLUGIN_FLOW_TRIGGER_TIMER_CHECK_GENERIC_FN(             \
        plugin_name)(const pcm_flow_t flow, const struct signal_attr *attr) {  \
        struct plugin_name##_flow *flow_ctx =                                  \
            (struct plugin_name##_flow *)(flow->backend_ctx);                  \
        pcm_uint timer = flow_ctx->signals[attr->metadata.index];              \
        pcm_uint threshold = flow_ctx->thresholds[attr->metadata.index];       \
        if (timer) {                                                           \
            pcm_uint now = time_diff_fn(flow_ctx->start_ts, time_now_fn());    \
            /* we assume that now is always larger than timer value */         \
            if (now - timer >= threshold) {                                    \
                PCM_LOG_DBG("TIMER EXPIRED: now=%d timer=%d threshold=%d",     \
                            diff, timer, threshold);                           \
                return true;                                                   \
            }                                                                  \
        }                                                                      \
        return false;                                                          \
    }

/* post reset state of 1 indicates that timer got (re-activated) */
#define PLUGIN_FLOW_TRIGGER_TIMER_RESET_GENERIC_FN(plugin_name)                \
    plugin_name##_flow_trigger_timer_reset
#define PLUGIN_FLOW_TRIGGER_TIMER_RESET_GENERIC_DEFINE(                        \
    plugin_name, time_now_fn, time_diff_fn)                                    \
    static inline void PLUGIN_FLOW_TRIGGER_TIMER_RESET_GENERIC_FN(             \
        plugin_name)(pcm_flow_t flow, const struct signal_attr *attr) {        \
        struct plugin_name##_flow *flow_ctx =                                  \
            (struct plugin_name##_flow *)(flow->backend_ctx);                  \
        pcm_uint timer = flow_ctx->signals[attr->metadata.index];              \
        if (timer == PCM_SIG_REARM) {                                          \
            flow_ctx->signals[attr->metadata.index] =                          \
                time_diff_fn(flow_ctx->start_ts, time_now_fn());               \
        }                                                                      \
    }

#define PLUGIN_FLOW_TIME_GET_GENERIC_FN(plugin_name) plugin_name##_flow_time_get
#define PLUGIN_FLOW_TIME_GET_GENERIC_DEFINE(plugin_name, time_now_fn,          \
                                            time_diff_fn)                      \
    static inline pcm_uint PLUGIN_FLOW_TIME_GET_GENERIC_FN(plugin_name)(       \
        const pcm_flow_t flow) {                                               \
        struct plugin_name##_flow *flow_ctx =                                  \
            (struct plugin_name##_flow *)(flow->backend_ctx);                  \
        return time_diff_fn(flow_ctx->start_ts, time_now_fn());                \
    }
#endif /* _UTIL_H_ */