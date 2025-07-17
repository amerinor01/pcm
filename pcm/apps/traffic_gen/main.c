#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "algo_utils.h"
#include "pcm.h"
#include "pcm_network.h"
#include "traffic_gen.h"

int main(int argc, char **argv) {
    EXIT_ON_ERR(argc != 4, 0);
    int num_flows = atoi(argv[1]);
    int test_duration = atoi(argv[2]);
    char *algo_name = argv[3];

    device_t *dev_ctx;
    EXIT_ON_ERR(device_init("pthread", &dev_ctx), PCM_SUCCESS);

    pcm_handle_t pcmc;
    EXIT_ON_ERR(device_pcmc_init(dev_ctx, algo_name, &pcmc), 0);
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

    EXIT_ON_ERR(device_pcmc_destroy(pcmc), 0);

    EXIT_ON_ERR(device_destroy(dev_ctx), PCM_SUCCESS);

    return 0;
}