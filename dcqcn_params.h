#ifndef DCQCN_PARAMS_H
#define DCQCN_PARAMS_H

#include <stdint.h>

typedef struct {
    uint32_t k_min;    /* no-mark waterline (bytes) */
    uint32_t k_max;    /* full-mark waterline (bytes) */
    uint32_t rate_max; /* max rate (bytes/sec) */
} dcqcn_params_t;

#endif /* DCQCN_PARAMS_H */