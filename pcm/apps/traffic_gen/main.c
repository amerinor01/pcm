#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "algo_utils.h"
#include "traffic_gen.h"
#include "dcqcn.h"
#include "fabric_params.h"
#include "pcm_network.h"
#include "pcm.h"
#include "smartt.h"
#include "swift.h"
#include "tcp.h"

int pcmc_init(const char *algo_name, device_t *dev_ctx,
              const char *reno_handler_path, pcm_handle_t *algo_handler) {

    pcm_handle_t new_handle;
    EXIT_ON_ERR(register_pcmc((void *)dev_ctx, 0, 0, 0, 0, &new_handle),
                PCM_SUCCESS);

    if (!strcmp(algo_name, "newreno")) {
        fprintf(stdout, "Algorithm requested: NewReno\n");
        EXIT_ON_ERR(tcp_pcmc_init(new_handle), PCM_SUCCESS);
    } else if (!strcmp(algo_name, "dctcp")) {
        fprintf(stdout, "Algorithm requested: DCTCP\n");
        EXIT_ON_ERR(tcp_pcmc_init(new_handle), PCM_SUCCESS);
        EXIT_ON_ERR(dctcp_pcmc_init(new_handle), PCM_SUCCESS);
    } else if (!strcmp(algo_name, "swift")) {
        fprintf(stdout, "Algorithm requested: Swift\n");
        EXIT_ON_ERR(swift_pcmc_init(new_handle), PCM_SUCCESS);
    } else if (!strcmp(algo_name, "dcqcn")) {
        fprintf(stdout, "Algorithm requested: DCQCN\n");
        EXIT_ON_ERR(dcqcn_pcmc_init(new_handle), PCM_SUCCESS);
    } else if (!strcmp(algo_name, "smartt")) {
        fprintf(stdout, "Algorithm requested: SMaRTT\n");
        EXIT_ON_ERR(smartt_pcmc_init(new_handle), PCM_SUCCESS);
    } else {
        fprintf(stderr, "Unknown algorithm name %s\n", algo_name);
        exit(EXIT_FAILURE);
    }

    char *compile_out;
    EXIT_ON_ERR(
        register_algorithm_pcmc(reno_handler_path, &compile_out, new_handle),
        PCM_SUCCESS);

    EXIT_ON_ERR(activate_pcmc(new_handle), PCM_SUCCESS);

    *algo_handler = new_handle;

    return 0;
}

int pcmc_destroy(pcm_handle_t algo_handler) {
    EXIT_ON_ERR(deactivate_pcmc(algo_handler), PCM_SUCCESS);
    EXIT_ON_ERR(deregister_pcmc(algo_handler), PCM_SUCCESS);
    return 0;
}

int main(int argc, char **argv) {
    EXIT_ON_ERR(argc != 5, 0);
    int num_flows = atoi(argv[1]);
    int test_duration = atoi(argv[2]);
    char *algo_name = argv[3];
    char *handler_path = argv[4];

    device_t *dev_ctx;
    EXIT_ON_ERR(device_init("pthread", &dev_ctx), PCM_SUCCESS);

    pcm_handle_t pcmc;
    EXIT_ON_ERR(pcmc_init(algo_name, dev_ctx, handler_path, &pcmc), 0);
    flow_t **flows = calloc(num_flows, sizeof(flow_t *));
    if (!flows) {
        fprintf(stderr, "Failed to allocate memory for flow pointers\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_flows; i++)
        EXIT_ON_ERR(flow_create(dev_ctx, &flows[i], &app_flow_traffic_gen_fn),
                    PCM_SUCCESS);

    /* Traffic flows */
    usleep(test_duration);

    for (int i = 0; i < num_flows; i++)
        EXIT_ON_ERR(flow_destroy(flows[i]), PCM_SUCCESS);
    free(flows);

    EXIT_ON_ERR(pcmc_destroy(pcmc), 0);

    EXIT_ON_ERR(device_destroy(dev_ctx), PCM_SUCCESS);

    return 0;
}