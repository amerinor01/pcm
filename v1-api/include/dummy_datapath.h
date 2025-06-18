#ifndef DUMMY_DATAPATH_H
#define DUMMY_DATAPATH_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>

#include "pcc.h"

struct dummy_flow;

int dummy_flow_create(struct pcc_dev_ctx *dev, struct dummy_flow **flow);
int dummy_flow_destroy(struct dummy_flow *flow);

#endif /* DUMMY_DATAPATH_H */