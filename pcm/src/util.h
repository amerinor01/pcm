#ifndef _UTIL_H_
#define _UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include "impl.h"
#include "lwlog.h"

#define CLOCK_GETTIME_TS_DIFF_GET(tp_start, tp_end)                            \
    ((((double)tp_end.tv_sec * 1e9 + (double)tp_end.tv_nsec) -                 \
      ((double)tp_start.tv_sec * 1e9 + (double)tp_start.tv_nsec)) /            \
     1e3)

#define LOG_DBG(FORMAT, ...)                                                   \
    {                                                                          \
        lwlog_debug(FORMAT, ##__VA_ARGS__);                                    \
    }

#define LOG_INFO(FORMAT, ...)                                                  \
    {                                                                          \
        lwlog_info(FORMAT, ##__VA_ARGS__);                                     \
    }

#define LOG_CRIT(FORMAT, ...)                                                  \
    {                                                                          \
        lwlog_crit(FORMAT, ##__VA_ARGS__);                                     \
    }

#define LOG_PRINT(FORMAT, ...)                                                 \
    {                                                                          \
        lwlog_info(FORMAT, ##__VA_ARGS__);                                     \
    }

#define LOG_FATAL(FORMAT, ...)                                                 \
    {                                                                          \
        lwlog_crit(FORMAT, ##__VA_ARGS__);                                     \
        exit(EXIT_FAILURE);                                                    \
    }

#define container_of(ptr, type, member)                                        \
    ({                                                                         \
        const typeof(((type *)0)->member) *__mptr = (ptr);                     \
        (type *)((char *)__mptr - offsetof(type, member));                     \
    })

#define ATTR_LIST_ITEM_ALLOC(attr_list, user_index, item_counter, max_items,   \
                             attr_ptr)                                         \
    {                                                                          \
        if ((item_counter) >= (max_items)) {                                   \
            LOG_CRIT("[attr_list=%p] %s list storage is full", attr_list,      \
                     #attr_list);                                              \
            return ERROR;                                                      \
        }                                                                      \
        if ((user_index) >= (max_items)) {                                     \
            LOG_CRIT("[attr_list=%p] user_index exeeds %s list capacity",      \
                     attr_list, #attr_list);                                   \
            return ERROR;                                                      \
        }                                                                      \
        (attr_ptr) = calloc(1, sizeof(*(attr_ptr)));                           \
        if (!(attr_ptr)) {                                                     \
            LOG_CRIT("[attr_list=%p] failed to allocate new attribute",        \
                     attr_list);                                               \
            return ERROR;                                                      \
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
                LOG_CRIT("[attr_list=%p] found duplicate index=%zu",           \
                         attr_list, user_index);                               \
                return ERROR;                                                  \
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
                LOG_CRIT("[attr_list=%p] found duplicate type=%d", attr_list,  \
                         chk_type);                                            \
                return ERROR;                                                  \
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
            LOG_CRIT("[attr_list=%p] item set on an empty list at index=%zu",  \
                     attr_list, user_index);                                   \
            return ERROR;                                                      \
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
            LOG_CRIT("[attr_list=%p] failed to find attribute with index=%zu", \
                     attr_list, user_index);                                   \
            return ERROR;                                                      \
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

static inline const char *signal_type_to_string(signal_t type) {
    switch (type) {
    case SIG_ACK:
        return "SIG_ACK";
    case SIG_RTO:
        return "SIG_RTO";
    case SIG_NACK:
        return "SIG_NACK";
    case SIG_ECN:
        return "SIG_ECN";
    case SIG_RTT:
        return "SIG_RTT";
    case SIG_DATA_TX:
        return "SIG_DATA_TX";
    case SIG_ELAPSED_TIME:
        return "SIG_ELAPSED_TIME";
    default:
        return "SIG_UNKNOWN";
    }
    return "SIG_ACCUM_UNKNOWN";
}

static inline const char *signal_accum_type_to_string(signal_accum_t type) {
    switch (type) {
    case SIG_ACCUM_SUM:
        return "SIG_ACCUM_SUM";
    case SIG_ACCUM_MIN:
        return "SIG_ACCUM_MIN";
    case SIG_ACCUM_MAX:
        return "SIG_ACCUM_MAX";
    case SIG_ACCUM_LAST:
        return "SIG_ACCUM_LAST";
    default:
        return "SIG_ACCUM_UNKNOWN";
    }
    return "SIG_ACCUM_UNKNOWN";
}

#define STATIC_ASSERT _Static_assert

STATIC_ASSERT(sizeof(float) <= sizeof(uint64_t),
              "float datatype must fit into 64 bits");
STATIC_ASSERT(sizeof(int) <= sizeof(uint64_t),
              "int datatype must fit into 64 bits");

/* encode any value T into 64-bit representation */
static inline uint64_t encode_u64(const void *val, size_t val_size) {
    uint64_t u;
    memcpy(&u, val, val_size);
    return u;
}

/* decode a 64-bit raw into a value T */
static inline void decode_u64(uint64_t u, void *out, size_t out_size) {
    memcpy(out, &u, out_size);
}

static inline float decode_float(uint64_t u) {
    float f;
    decode_u64(u, &f, sizeof(f));
    return f;
}

static inline int decode_int(uint64_t u) {
    int x;
    decode_u64(u, &x, sizeof(x));
    return x;
}

static inline uint64_t encode_float(float f) {
    return encode_u64(&f, sizeof(f));
}

static inline uint64_t encode_int(uint32_t x) {
    return encode_u64(&x, sizeof(x));
}

#endif /* _UTIL_H_ */