#ifndef _ALGO_UTILS_H_
#define _ALGO_UTILS_H_

#include <stdio.h>
#include <stdlib.h>

#define MAX(a, b)                                                              \
    ({                                                                         \
        __typeof__(a) _a = (a);                                                \
        __typeof__(b) _b = (b);                                                \
        _a > _b ? _a : _b;                                                     \
    })

#define MIN(a, b)                                                              \
    ({                                                                         \
        __typeof__(a) _a = (a);                                                \
        __typeof__(b) _b = (b);                                                \
        _a < _b ? _a : _b;                                                     \
    })

#define MIN_NOT_ZERO(x, y)                                                     \
    ({                                                                         \
        typeof(x) __x = (x);                                                   \
        typeof(y) __y = (y);                                                   \
        __x == 0 ? __y : ((__y == 0) ? __x : MIN(__x, __y));                   \
    })

#define ABS(x) ((x) < 0 ? -(x) : (x))

#define EXIT_ON_ERR(call, success_errcode)                                     \
    do {                                                                       \
        int _err = (call);                                                     \
        if (_err != success_errcode) {                                         \
            fprintf(stderr, "Call '%s' failed with code %d at %s:%d\n", #call, \
                    _err, __FILE__, __LINE__);                                 \
            exit(EXIT_FAILURE);                                                \
        }                                                                      \
    } while (0)

#endif /* _ALGO_UTILS_H_ */