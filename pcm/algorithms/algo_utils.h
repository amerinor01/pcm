#ifndef _ALGO_UTILS_H_
#define _ALGO_UTILS_H_

#include <stdio.h> // all handlers can call to printf to debug
#include <xxhash.h>
#include <math.h>

#define HASH(a)                                                                                                                            \
    ({                                                                                                                                     \
        pcm_uint __hash_tmp = (a);                                                                                                         \
        XXH64(&__hash_tmp, sizeof(__hash_tmp), 0);                                                                                         \
    })

#define RAND()                                                                                                                             \
    ({                                                                                                                                     \
        /* this is dirty because it assumes that snapshot object exist at the calling point */                                             \
        uintptr_t __tmp = (uintptr_t)snapshot;                                                                                             \
        XXH64(&__tmp, sizeof(__tmp), 0);                                                                                                   \
    })

#define MAX(a, b)                                                                                                                          \
    ({                                                                                                                                     \
        __typeof__(a) _a = (a);                                                                                                            \
        __typeof__(b) _b = (b);                                                                                                            \
        _a > _b ? _a : _b;                                                                                                                 \
    })

#define MIN(a, b)                                                                                                                          \
    ({                                                                                                                                     \
        __typeof__(a) _a = (a);                                                                                                            \
        __typeof__(b) _b = (b);                                                                                                            \
        _a < _b ? _a : _b;                                                                                                                 \
    })

#define MIN_NOT_ZERO(x, y)                                                                                                                 \
    ({                                                                                                                                     \
        typeof(x) __x = (x);                                                                                                               \
        typeof(y) __y = (y);                                                                                                               \
        __x == 0 ? __y : ((__y == 0) ? __x : MIN(__x, __y));                                                                               \
    })

#define ABS(x) ((x) < 0 ? -(x) : (x))
#define POW(x, y) (pow((x), (y)))

// Bitmap manipulation helpers
#define BITS_PER_UINT (sizeof(pcm_uint) * 8)

#define BITMAP_HELPERS_DEFINE(bitmap_name)                                                                                                 \
    static PCM_FORCE_INLINE void bitmap_name##_set_bitmap_entry(ALGO_CTX_ARGS, pcm_uint bit_idx, pcm_uint bit) {                           \
        pcm_uint word_idx = bit_idx / BITS_PER_UINT;                                                                                       \
        pcm_uint bit_offset = bit_idx % BITS_PER_UINT;                                                                                     \
        pcm_uint mask = 1ULL << bit_offset;                                                                                                \
        pcm_uint word = get_arr_uint(bitmap_name, word_idx);                                                                               \
        if (bit)                                                                                                                           \
            word |= mask;                                                                                                                  \
        else                                                                                                                               \
            word &= ~mask;                                                                                                                 \
        set_arr_uint(bitmap_name, word_idx, word);                                                                                         \
    }                                                                                                                                      \
    static PCM_FORCE_INLINE pcm_uint bitmap_name##_get_bitmap_entry(ALGO_CTX_ARGS, pcm_uint bit_idx) {                                     \
        pcm_uint word_idx = bit_idx / BITS_PER_UINT;                                                                                       \
        pcm_uint bit_offset = bit_idx % BITS_PER_UINT;                                                                                     \
        pcm_uint mask = 1ULL << bit_offset;                                                                                                \
        pcm_uint word = get_arr_uint(bitmap_name, word_idx);                                                                               \
        return word & mask;                                                                                                                \
    }

#define BITMAP_HELPER_SET_ENTRY(bitmap_name, bit_idx, bit) (bitmap_name##_set_bitmap_entry(ALGO_CTX_PASS, bit_idx, bit))
#define BITMAP_HELPER_GET_ENTRY(bitmap_name, bit_idx) (bitmap_name##_get_bitmap_entry(ALGO_CTX_PASS, bit_idx))

#endif /* _ALGO_UTILS_H_ */