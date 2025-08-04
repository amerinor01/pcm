#ifndef _UTIL_H_
#define _UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include "impl.h"
#include "pcm_log.h"

#define UTIL_MASK_TO_ARR_IDX(mask) ((unsigned long)__builtin_ctz(mask))

#define container_of(ptr, type, member)                                        \
    ({                                                                         \
        const typeof(((type *)0)->member) *__mptr = (ptr);                     \
        (type *)((char *)__mptr - offsetof(type, member));                     \
    })

#define ATTR_LIST_ITEM_ALLOC(attr_list, idx, item_counter, max_items,          \
                             attr_ptr, idx_is_mask)                            \
    {                                                                          \
        if ((item_counter) >= (max_items)) {                                   \
            PCM_LOG_CRIT("[attr_list=%p] %s list storage is full", attr_list,  \
                         #attr_list);                                          \
            return PCM_ERROR;                                                  \
        }                                                                      \
        if (!(idx_is_mask) && (idx) >= (max_items)) {                          \
            PCM_LOG_CRIT(                                                      \
                "[attr_list=%p] absolute idx=%zu exeeds %s list capacity",     \
                attr_list, idx, #attr_list);                                   \
            return PCM_ERROR;                                                  \
        } else if ((idx_is_mask) &&                                            \
                   ((UTIL_MASK_TO_ARR_IDX(idx)) >= (max_items))) {             \
            PCM_LOG_CRIT(                                                      \
                "[attr_list=%p] mask idx=%zu exeeds %s list capacity",         \
                attr_list, UTIL_MASK_TO_ARR_IDX(idx), #attr_list);             \
            return PCM_ERROR;                                                  \
        }                                                                      \
        (attr_ptr) = calloc(1, sizeof(*(attr_ptr)));                           \
        if (!(attr_ptr)) {                                                     \
            PCM_LOG_CRIT("[attr_list=%p] failed to allocate new attribute",    \
                         attr_list);                                           \
            return PCM_ERROR;                                                  \
        }                                                                      \
        (attr_ptr)->metadata.idx = (idx);                                      \
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

#define ATTR_LIST_DUPLICATE_IDX_CHK(attr_list, attr_type, idx)                 \
    {                                                                          \
        struct slist_entry *item, *prev;                                       \
        slist_foreach(attr_list, item, prev) {                                 \
            (void)prev; /* suppress complier warning */                        \
            if (container_of(item, attr_type, metadata.list_entry)             \
                    ->metadata.idx == (idx)) {                                 \
                PCM_LOG_CRIT("[attr_list=%p] found duplicate idx=%zu",         \
                             attr_list, idx);                                  \
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
                (found_idx) = attr->metadata.idx;                              \
                break;                                                         \
            }                                                                  \
        }                                                                      \
    }

#define ATTR_LIST_ITEM_SET(attr_list, attr_type, idx, val, found_attr_ptr)     \
    {                                                                          \
        if (slist_empty(attr_list)) {                                          \
            PCM_LOG_CRIT(                                                      \
                "[attr_list=%p] item set on an empty list at idx=%zu",         \
                attr_list, idx);                                               \
            return PCM_ERROR;                                                  \
        }                                                                      \
        (found_attr_ptr) = NULL;                                               \
        attr_type *cur_attr = NULL;                                            \
        struct slist_entry *item, *prev;                                       \
        slist_foreach(attr_list, item, prev) {                                 \
            (void)prev; /* suppress complier warning */                        \
            cur_attr = container_of(item, attr_type, metadata.list_entry);     \
            if (cur_attr->metadata.idx == (idx)) {                             \
                (found_attr_ptr) = cur_attr;                                   \
                break;                                                         \
            }                                                                  \
        }                                                                      \
        if (!(found_attr_ptr)) {                                               \
            PCM_LOG_CRIT(                                                      \
                "[attr_list=%p] failed to find attribute with idx=%zu",        \
                attr_list, idx);                                               \
            return PCM_ERROR;                                                  \
        }                                                                      \
        (found_attr_ptr)->metadata.value = val;                                \
    }

#define ATTR_LIST_FLOW_STATE_INIT(attr_list, attr_type, state_ptr,             \
                                  idx_is_mask)                                 \
    {                                                                          \
        attr_type *cur_attr = NULL;                                            \
        struct slist_entry *item, *prev;                                       \
        slist_foreach(attr_list, item, prev) {                                 \
            (void)prev; /* suppress complier warning */                        \
            cur_attr = container_of(item, attr_type, metadata.list_entry);     \
            (state_ptr)[idx_is_mask                                            \
                            ? UTIL_MASK_TO_ARR_IDX(cur_attr->metadata.idx)     \
                            : cur_attr->metadata.idx] =                        \
                cur_attr->metadata.value;                                      \
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

#define PLUGIN_FLOW_CONTROL_GET_GENERIC_FN(plugin_name)                        \
    plugin_name##_flow_control_get
#define PLUGIN_FLOW_CONTROL_GET_GENERIC_DEFINE(plugin_name)                    \
    static inline pcm_uint PLUGIN_FLOW_CONTROL_GET_GENERIC_FN(plugin_name)(    \
        const pcm_flow_t ctx, size_t idx) {                                         \
        struct plugin_name##_flow *flow_ctx =                                  \
            ((struct plugin_name##_flow *)(((pcm_flow_t)ctx)->backend_ctx));   \
        return flow_ctx->controls[idx];                                        \
    }

#define PLUGIN_FLOW_CONTROL_SET_GENERIC_FN(plugin_name)                        \
    plugin_name##_flow_control_set
#define PLUGIN_FLOW_CONTROL_SET_GENERIC_DEFINE(plugin_name)                    \
    static inline void PLUGIN_FLOW_CONTROL_SET_GENERIC_FN(plugin_name)(        \
        pcm_flow_t ctx, size_t idx, pcm_uint val) {                                 \
        struct plugin_name##_flow *flow_ctx =                                  \
            ((struct plugin_name##_flow *)(((pcm_flow_t)ctx)->backend_ctx));   \
        flow_ctx->controls[idx] = val;                                         \
    }

#define PLUGIN_FLOW_ACCUMULATION_OP_SUM_GENERIC_FN(plugin_name)                \
    plugin_name##_flow_signal_accumulation_op_sum
#define PLUGIN_FLOW_ACCUMULATION_OP_SUM_GENERIC_DEFINE(plugin_name)            \
    static inline void PLUGIN_FLOW_ACCUMULATION_OP_SUM_GENERIC_FN(             \
        plugin_name)(pcm_flow_t flow, const struct signal_attr *attr,          \
                     pcm_uint signal) {                                        \
        struct plugin_name##_flow *flow_ctx =                                  \
            (struct plugin_name##_flow *)(flow->backend_ctx);                  \
        flow_ctx->signals[UTIL_MASK_TO_ARR_IDX(attr->metadata.idx)] += signal; \
    }

#define PLUGIN_FLOW_ACCUMULATION_OP_LAST_GENERIC_FN(plugin_name)               \
    plugin_name##_flow_signal_accumulation_op_last
#define PLUGIN_FLOW_ACCUMULATION_OP_LAST_GENERIC_DEFINE(plugin_name)           \
    static inline void PLUGIN_FLOW_ACCUMULATION_OP_LAST_GENERIC_FN(            \
        plugin_name)(pcm_flow_t flow, const struct signal_attr *attr,          \
                     pcm_uint signal) {                                        \
        struct plugin_name##_flow *flow_ctx =                                  \
            (struct plugin_name##_flow *)(flow->backend_ctx);                  \
        flow_ctx->signals[UTIL_MASK_TO_ARR_IDX(attr->metadata.idx)] = signal;  \
    }

#define PLUGIN_FLOW_ACCUMULATION_OP_MIN_GENERIC_FN(plugin_name)                \
    plugin_name##_flow_signal_accumulation_op_min
#define PLUGIN_FLOW_ACCUMULATION_OP_MIN_GENERIC_DEFINE(plugin_name)            \
    static inline void PLUGIN_FLOW_ACCUMULATION_OP_MIN_GENERIC_FN(             \
        plugin_name)(pcm_flow_t flow, const struct signal_attr *attr,          \
                     pcm_uint signal) {                                        \
        struct plugin_name##_flow *flow_ctx =                                  \
            (struct plugin_name##_flow *)(flow->backend_ctx);                  \
        if (signal <                                                           \
            flow_ctx->signals[UTIL_MASK_TO_ARR_IDX(attr->metadata.idx)]) {     \
            flow_ctx->signals[UTIL_MASK_TO_ARR_IDX(attr->metadata.idx)] =      \
                signal;                                                        \
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
        if (signal >                                                           \
            flow_ctx->signals[UTIL_MASK_TO_ARR_IDX(attr->metadata.idx)]) {     \
            flow_ctx->signals[UTIL_MASK_TO_ARR_IDX(attr->metadata.idx)] =      \
                signal;                                                        \
        }                                                                      \
    }

#define PLUGIN_FLOW_TRIGGER_OVERFLOW_CHECK_GENERIC_FN(plugin_name)             \
    plugin_name##_flow_trigger_overflow_check
#define PLUGIN_FLOW_TRIGGER_OVERFLOW_CHECK_GENERIC_DEFINE(plugin_name)         \
    static inline bool PLUGIN_FLOW_TRIGGER_OVERFLOW_CHECK_GENERIC_FN(          \
        plugin_name)(const pcm_flow_t flow, const struct signal_attr *attr) {  \
        struct plugin_name##_flow *flow_ctx =                                  \
            (struct plugin_name##_flow *)(flow->backend_ctx);                  \
        pcm_uint value =                                                       \
            flow_ctx->signals[UTIL_MASK_TO_ARR_IDX(attr->metadata.idx)];       \
        pcm_uint threshold =                                                   \
            flow_ctx->thresholds[UTIL_MASK_TO_ARR_IDX(attr->metadata.idx)];    \
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
        pcm_uint burst =                                                       \
            flow_ctx->signals[UTIL_MASK_TO_ARR_IDX(attr->metadata.idx)];       \
        if (burst) {                                                           \
            flow_ctx->signals[UTIL_MASK_TO_ARR_IDX(attr->metadata.idx)] = 1;   \
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
        pcm_uint burst =                                                       \
            flow_ctx->signals[UTIL_MASK_TO_ARR_IDX(attr->metadata.idx)];       \
        pcm_uint threshold =                                                   \
            flow_ctx->thresholds[UTIL_MASK_TO_ARR_IDX(attr->metadata.idx)];    \
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
        flow_ctx->signals[UTIL_MASK_TO_ARR_IDX(attr->metadata.idx)] =          \
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
        pcm_uint timer =                                                       \
            flow_ctx->signals[UTIL_MASK_TO_ARR_IDX(attr->metadata.idx)];       \
        pcm_uint threshold =                                                   \
            flow_ctx->thresholds[UTIL_MASK_TO_ARR_IDX(attr->metadata.idx)];    \
        if (timer) {                                                           \
            pcm_uint now = time_diff_fn(flow_ctx->start_ts, time_now_fn());    \
            /* we assume that now is always larger than timer value */         \
            if (now - timer >= threshold) {                                    \
                PCM_LOG_DBG("TIMER EXPIRED: now=%d timer=%d threshold=%d",     \
                            now, timer, threshold);                            \
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
        pcm_uint timer =                                                       \
            flow_ctx->signals[UTIL_MASK_TO_ARR_IDX(attr->metadata.idx)];       \
        if (timer == PCM_SIG_REARM) {                                          \
            flow_ctx->signals[UTIL_MASK_TO_ARR_IDX(attr->metadata.idx)] =      \
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