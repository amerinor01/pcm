#ifndef DUMMY_DATAPATH_H
#define DUMMY_DATAPATH_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>

#include "dummy_datapath.h"

#define DEFAULT_BANDWIDTH_BPS 100000000UL  // 100 Mbps
#define DEFAULT_DROP_PROB       0.01       // 1% packet drop probability
#define DEFAULT_NACK_PROB       0.02       // 2% NACK probability (duplicate ACK)
#define DEFAULT_PACKET_SIZE     1500       // bytes per packet (MSS)

struct dummy_signal_storage {
    struct pcc_algorithm_handler_obj *handler;
    atomic_uint_fast64_t *signals_u64;
    void *user_data;
    size_t num_signals;
    int cwnd_idx;
    int acks_idx;
    int nacks_idx;
    int rto_idx;
};

struct dummy_flow {
    struct pcc_dev_ctx *dev;
    struct pcc_flow_ctx *pcc_ctx;
    atomic_bool running;
    pthread_t thread;
    int fid;
    struct test_flow *next;
};

#endif /* DUMMY_DATAPATH_H */