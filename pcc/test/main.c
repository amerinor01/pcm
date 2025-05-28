#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include "network.h"
#include "pcm.h"
#include "reno.h"

#define EXIT_ON_ERR(call, success_errcode)                                     \
    do {                                                                       \
        int _err = (call);                                                     \
        if (_err != success_errcode) {                                         \
            fprintf(stderr, "Call '%s' failed with code %d at %s:%d\n", #call, \
                    _err, __FILE__, __LINE__);                                 \
            exit(EXIT_FAILURE);                                                \
        }                                                                      \
    } while (0)

const int acked_init = 0;
const int ssthresh_init = INT_MAX;
const int cwnd_init = 1;

int reno_algo_init(device_t *dev_ctx, const char *reno_handler_path,
                   handle_t *algo_handler) {

    handle_t new_handle;
    EXIT_ON_ERR(register_pcmc((void *)dev_ctx, UINT32_MAX, UINT32_MAX,
                              UINT32_MAX, UINT32_MAX, &new_handle),
                SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(SIG_ACK, SIG_ACCUM_SUM, RENO_SIG_IDX_ACK,
                                     new_handle),
                SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(RENO_SIG_IDX_ACK, 1, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(SIG_RTO, SIG_ACCUM_SUM, RENO_SIG_IDX_RTO,
                                     new_handle),
                SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(RENO_SIG_IDX_RTO, 1, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(SIG_NACK, SIG_ACCUM_SUM, RENO_SIG_IDX_NACK,
                                     new_handle),
                SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(RENO_SIG_IDX_NACK, 1, new_handle),
        SUCCESS);

    EXIT_ON_ERR(
        register_control_pcmc(CTRL_CWND, RENO_CTRL_IDX_CWND, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_control_initial_value_pcmc(RENO_CTRL_IDX_CWND,
                                                    cwnd_init, new_handle),
                SUCCESS);

    EXIT_ON_ERR(
        register_local_state_pcmc(RENO_LOCAL_STATE_IDX_SSTHRESH, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(
                    RENO_LOCAL_STATE_IDX_SSTHRESH, ssthresh_init, new_handle),
                SUCCESS);

    EXIT_ON_ERR(
        register_local_state_pcmc(RENO_LOCAL_STATE_IDX_ACKED, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(
                    RENO_LOCAL_STATE_IDX_ACKED, acked_init, new_handle),
                SUCCESS);

    char *compile_out;
    EXIT_ON_ERR(
        register_algorithm_pcmc(reno_handler_path, &compile_out, new_handle),
        SUCCESS);

    EXIT_ON_ERR(activate_pcmc(new_handle), SUCCESS);

    *algo_handler = new_handle;

    return 0;
}

int reno_algo_destroy(handle_t algo_handler) {
    EXIT_ON_ERR(deactivate_pcmc(algo_handler), SUCCESS);
    EXIT_ON_ERR(deregister_pcmc(algo_handler), SUCCESS);
    return 0;
}

int main(int argc, char **argv) {
    EXIT_ON_ERR(argc != 4, 0);
    int num_flows = atoi(argv[1]);
    int test_duration = atoi(argv[2]);
    char *reno_handler_path = argv[3];

    device_t *dev_ctx;
    EXIT_ON_ERR(device_init(NULL, &dev_ctx), SUCCESS);

    handle_t reno_pcmc;
    EXIT_ON_ERR(reno_algo_init(dev_ctx, reno_handler_path, &reno_pcmc), 0);

    flow_t **flows = calloc(num_flows, sizeof(flow_t *));
    if (!flows) {
        fprintf(stderr, "Failed to allocate memory for flow pointers\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_flows; i++)
        EXIT_ON_ERR(flow_create(dev_ctx, &flows[i], NULL), SUCCESS);

    /* Traffic flows */
    usleep(test_duration);

    for (int i = 0; i < num_flows; i++)
        EXIT_ON_ERR(flow_destroy(flows[i]), SUCCESS);

    EXIT_ON_ERR(reno_algo_destroy(reno_pcmc), 0);
    EXIT_ON_ERR(device_destroy(dev_ctx), SUCCESS);

    return 0;
}