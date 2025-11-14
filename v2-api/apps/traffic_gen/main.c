#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pcm_log.h"
#include "pcm.h"
#include "pcm_network.h"
#include "traffic_gen.h"

int main(int argc, char **argv) {
    if (argc != 4)
        PCM_LOG_FATAL("Usage: ./traffic_gen_app <num_flows> <flow_duration> <algorithm_name>");
    int num_flows = atoi(argv[1]);
    int test_duration = atoi(argv[2]);
    char *algo_name = argv[3];

    pcm_device_t dev_ctx;
    PCM_EXIT_ON_ERR(device_init("pthread", &dev_ctx));

    pcm_handle_t pcmc;
    PCM_EXIT_ON_ERR(device_pcmc_init(dev_ctx, algo_name, &pcmc));
    pcm_flow_t *flows = calloc(num_flows, sizeof(pcm_flow_t));
    if (!flows) {
        fprintf(stderr, "Failed to allocate memory for flow pointers\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_flows; i++)
        PCM_EXIT_ON_ERR(flow_create(dev_ctx, &flows[i], &app_flow_traffic_gen_fn));

    /* Traffic flows */
    usleep(test_duration);

    for (int i = 0; i < num_flows; i++)
        PCM_EXIT_ON_ERR(flow_destroy(flows[i]));
    free(flows);

    PCM_EXIT_ON_ERR(device_pcmc_destroy(pcmc));

    PCM_EXIT_ON_ERR(device_destroy(dev_ctx));

    return 0;
}