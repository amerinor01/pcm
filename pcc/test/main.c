#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "dummy_datapath.h"
#include "pcc.h"
#include "reno.h"

#define EXIT_ON_ERR(call)                                                      \
    do {                                                                       \
        enum pcc_error_codes _err = (call);                                    \
        if (_err) {                                                            \
            fprintf(stderr, "Call '%s' failed with code %d at %s:%d\n", #call, \
                    _err, __FILE__, __LINE__);                                 \
            exit(EXIT_FAILURE);                                                \
        }                                                                      \
    } while (0)

int main(int argc, char **argv) {
    EXIT_ON_ERR(argc != 3);
    int num_flows = atoi(argv[1]);
    int test_duration = atoi(argv[2]);

    struct pcc_dev_ctx *dev_ctx;
    EXIT_ON_ERR(pcc_dev_ctx_init(NULL, &dev_ctx));

    struct pcc_algorithm_handler_obj *reno;
    EXIT_ON_ERR(reno_algo_init(dev_ctx, &reno));

    struct dummy_flow **flows =
        calloc(num_flows, sizeof(struct pcc_flow_ctx *));
    EXIT_ON_ERR((!flows));
    for (int i = 0; i < num_flows; i++)
        EXIT_ON_ERR(dummy_flow_create(dev_ctx, &flows[i]));

    /* Traffic flows */
    usleep(test_duration);

    for (int i = 0; i < num_flows; i++)
        EXIT_ON_ERR(dummy_flow_destroy(flows[i]));

    EXIT_ON_ERR(reno_algo_destroy(reno));
    EXIT_ON_ERR(pcc_dev_ctx_destroy(dev_ctx));
    return 0;
}