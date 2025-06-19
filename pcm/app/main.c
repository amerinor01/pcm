#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dcqcn.h"
#include "fabric_params.h"
#include "network.h"
#include "pcm.h"
#include "smartt.h"
#include "swift.h"
#include "tcp.h"

#define EXIT_ON_ERR(call, success_errcode)                                     \
    do {                                                                       \
        int _err = (call);                                                     \
        if (_err != success_errcode) {                                         \
            fprintf(stderr, "Call '%s' failed with code %d at %s:%d\n", #call, \
                    _err, __FILE__, __LINE__);                                 \
            exit(EXIT_FAILURE);                                                \
        }                                                                      \
    } while (0)

int tcp_pcmc_config(handle_t new_handle) {
    EXIT_ON_ERR(register_signal_pcmc(SIG_ACK, SIG_ACCUM_SUM, TCP_SIG_IDX_ACK,
                                     new_handle),
                SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(TCP_SIG_IDX_ACK, 1, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(SIG_RTO, SIG_ACCUM_SUM, TCP_SIG_IDX_RTO,
                                     new_handle),
                SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(TCP_SIG_IDX_RTO, 1, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(SIG_NACK, SIG_ACCUM_SUM, TCP_SIG_IDX_NACK,
                                     new_handle),
                SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(TCP_SIG_IDX_NACK, 1, new_handle),
        SUCCESS);

    EXIT_ON_ERR(register_control_pcmc(CTRL_CWND, TCP_CTRL_IDX_CWND, new_handle),
                SUCCESS);
    EXIT_ON_ERR(register_control_initial_value_pcmc(
                    TCP_CTRL_IDX_CWND, FABRIC_MIN_CWND, new_handle),
                SUCCESS);

    EXIT_ON_ERR(
        register_local_state_pcmc(TCP_LOCAL_STATE_IDX_SSTHRESH, new_handle),
        SUCCESS);
    EXIT_ON_ERR(
        register_local_state_initial_value_pcmc(TCP_LOCAL_STATE_IDX_SSTHRESH,
                                                TCP_SSTHRESH_INIT, new_handle),
        SUCCESS);

    EXIT_ON_ERR(
        register_local_state_pcmc(TCP_LOCAL_STATE_IDX_ACKED, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(
                    TCP_LOCAL_STATE_IDX_ACKED, 0, new_handle),
                SUCCESS);

    EXIT_ON_ERR(register_local_state_pcmc(TCP_LOCAL_STATE_IDX_IN_FAST_RECOV,
                                          new_handle),
                SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(
                    TCP_LOCAL_STATE_IDX_IN_FAST_RECOV, 0, new_handle),
                SUCCESS);
    return SUCCESS;
}

int dctcp_pcmc_init(handle_t new_handle) {
    EXIT_ON_ERR(register_signal_pcmc(SIG_ECN, SIG_ACCUM_SUM, TCP_SIG_IDX_ECN,
                                     new_handle),
                SUCCESS);
    EXIT_ON_ERR(register_local_state_pcmc(TCP_LOCAL_STATE_IDX_EPOCH_DELIVERED,
                                          new_handle),
                SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(
                    TCP_LOCAL_STATE_IDX_EPOCH_DELIVERED, 0, new_handle),
                SUCCESS);
    EXIT_ON_ERR(register_local_state_pcmc(
                    TCP_LOCAL_STATE_IDX_EPOCH_ECN_DELIVERED, new_handle),
                SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(
                    TCP_LOCAL_STATE_IDX_EPOCH_ECN_DELIVERED, 0, new_handle),
                SUCCESS);
    EXIT_ON_ERR(
        register_local_state_pcmc(TCP_LOCAL_STATE_IDX_ALPHA, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(
                    TCP_LOCAL_STATE_IDX_ALPHA, DCTCP_MAX_ALPHA, new_handle),
                SUCCESS);
    return SUCCESS;
}

int swift_pcmc_init(handle_t new_handle) {
    EXIT_ON_ERR(register_signal_pcmc(SIG_ACK, SIG_ACCUM_SUM, SWIFT_SIG_IDX_ACK,
                                     new_handle),
                SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(SWIFT_SIG_IDX_ACK, 1, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(SIG_RTO, SIG_ACCUM_SUM, SWIFT_SIG_IDX_RTO,
                                     new_handle),
                SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(SWIFT_SIG_IDX_RTO, 1, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(SIG_NACK, SIG_ACCUM_SUM,
                                     SWIFT_SIG_IDX_NACK, new_handle),
                SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(SWIFT_SIG_IDX_NACK, 1, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(SIG_RTT, SIG_ACCUM_LAST, SWIFT_SIG_IDX_RTT,
                                     new_handle),
                SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(SIG_ELAPSED_TIME, SIG_ACCUM_SUM,
                                     SIWFT_SIG_IDX_ELAPSED_TIME, new_handle),
                SUCCESS);

    EXIT_ON_ERR(
        register_control_pcmc(CTRL_CWND, SWIFT_CTRL_IDX_CWND, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_control_initial_value_pcmc(
                    SWIFT_CTRL_IDX_CWND, FABRIC_MIN_CWND, new_handle),
                SUCCESS);

    EXIT_ON_ERR(
        register_local_state_pcmc(SWIFT_LOCAL_STATE_IDX_ACKED, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(
                    SWIFT_LOCAL_STATE_IDX_ACKED, 0, new_handle),
                SUCCESS);

    EXIT_ON_ERR(register_local_state_pcmc(SWIFT_LOCAL_STATE_IDX_T_LAST_DECREASE,
                                          new_handle),
                SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(
                    SWIFT_LOCAL_STATE_IDX_T_LAST_DECREASE, 0, new_handle),
                SUCCESS);

    EXIT_ON_ERR(register_local_state_pcmc(SWIFT_LOCAL_STATE_IDX_RETRANSMIT_CNT,
                                          new_handle),
                SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(
                    SWIFT_LOCAL_STATE_IDX_RETRANSMIT_CNT, 0, new_handle),
                SUCCESS);

    EXIT_ON_ERR(
        register_local_state_pcmc(SWIFT_LOCAL_STATE_IDX_RTT_ESTIM, new_handle),
        SUCCESS);
    EXIT_ON_ERR(
        register_local_state_initial_value_pcmc(SWIFT_LOCAL_STATE_IDX_RTT_ESTIM,
                                                FABRIC_BASE_RTT, new_handle),
        SUCCESS);

    return SUCCESS;
}

int dcqcn_pcmc_init(handle_t new_handle) {
    EXIT_ON_ERR(register_signal_pcmc(SIG_RTT, SIG_ACCUM_LAST, DCQCN_SIG_IDX_RTT,
                                     new_handle),
                SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(SIG_ECN, SIG_ACCUM_SUM, DCQCN_SIG_IDX_ECN,
                                     new_handle),
                SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(DCQCN_SIG_IDX_ECN, 1, new_handle),
        SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(SIG_ELAPSED_TIME, SIG_ACCUM_SUM,
                                     DCQCN_SIG_IDX_ALPHA_TIMER, new_handle),
                SUCCESS);
    EXIT_ON_ERR(register_signal_invoke_trigger_pcmc(
                    DCQCN_SIG_IDX_ALPHA_TIMER, DCQCN_ALPHA_TIMER, new_handle),
                SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(SIG_ELAPSED_TIME, SIG_ACCUM_SUM,
                                     DCQCN_SIG_IDX_RATE_INCREASE_TIMER,
                                     new_handle),
                SUCCESS);
    EXIT_ON_ERR(register_signal_invoke_trigger_pcmc(
                    DCQCN_SIG_IDX_RATE_INCREASE_TIMER,
                    DCQCN_RATE_INCREASE_TIMER, new_handle),
                SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(SIG_DATA_TX, SIG_ACCUM_SUM,
                                     DCQCN_SIG_IDX_TX_BURST, new_handle),
                SUCCESS);
    EXIT_ON_ERR(register_signal_invoke_trigger_pcmc(
                    DCQCN_SIG_IDX_TX_BURST, DCQCN_BYTE_COUNTER, new_handle),
                SUCCESS);

    EXIT_ON_ERR(
        register_local_state_pcmc(DCQCN_LOCAL_STATE_IDX_ALPHA, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_float_pcmc(
                    DCQCN_LOCAL_STATE_IDX_ALPHA, DCQCN_ALPHA_INIT, new_handle),
                SUCCESS);

    EXIT_ON_ERR(
        register_local_state_pcmc(DCQCN_LOCAL_STATE_IDX_RATE_CUR, new_handle),
        SUCCESS);
    EXIT_ON_ERR(
        register_local_state_initial_value_float_pcmc(
            DCQCN_LOCAL_STATE_IDX_RATE_CUR, FABRIC_LINK_RATE_GBPS, new_handle),
        SUCCESS);

    EXIT_ON_ERR(register_local_state_pcmc(DCQCN_LOCAL_STATE_IDX_RATE_TARGET,
                                          new_handle),
                SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_float_pcmc(
                    DCQCN_LOCAL_STATE_IDX_RATE_TARGET, FABRIC_LINK_RATE_GBPS,
                    new_handle),
                SUCCESS);

    EXIT_ON_ERR(register_local_state_pcmc(
                    DCQCN_LOCAL_STATE_IDX_RATE_INCREASE_EVTS, new_handle),
                SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_int_pcmc(
                    DCQCN_LOCAL_STATE_IDX_RATE_INCREASE_EVTS, 0, new_handle),
                SUCCESS);

    EXIT_ON_ERR(register_local_state_pcmc(
                    DCQCN_LOCAL_STATE_IDX_BYTE_COUNTER_EVTS, new_handle),
                SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_int_pcmc(
                    DCQCN_LOCAL_STATE_IDX_BYTE_COUNTER_EVTS, 0, new_handle),
                SUCCESS);

    EXIT_ON_ERR(
        register_control_pcmc(CTRL_CWND, DCQCN_CTRL_IDX_CWND, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_control_initial_value_pcmc(
                    DCQCN_CTRL_IDX_CWND, FABRIC_MAX_CWND, new_handle),
                SUCCESS);
    return SUCCESS;
}

int smartt_pcmc_init(handle_t new_handle) {
    EXIT_ON_ERR(
        register_local_state_pcmc(SMARTT_LOCAL_STATE_ACKED_BYTES, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_int_pcmc(
                    SMARTT_LOCAL_STATE_ACKED_BYTES, 0, new_handle),
                SUCCESS);
    EXIT_ON_ERR(
        register_local_state_pcmc(SMARTT_LOCAL_STATE_BYTES_IGNORED, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_int_pcmc(
                    SMARTT_LOCAL_STATE_BYTES_IGNORED, 0, new_handle),
                SUCCESS);

    EXIT_ON_ERR(register_local_state_pcmc(SMARTT_LOCAL_STATE_BYTES_TO_IGNORE,
                                          new_handle),
                SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_int_pcmc(
                    SMARTT_LOCAL_STATE_BYTES_TO_IGNORE, 0, new_handle),
                SUCCESS);
    EXIT_ON_ERR(
        register_local_state_pcmc(SMARTT_LOCAL_STATE_TRIGGER_QA, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_int_pcmc(
                    SMARTT_LOCAL_STATE_TRIGGER_QA, 0, new_handle),
                SUCCESS);

    EXIT_ON_ERR(
        register_local_state_pcmc(SMARTT_LOCAL_STATE_QA_DEADLINE, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_int_pcmc(
                    SMARTT_LOCAL_STATE_QA_DEADLINE, 0, new_handle),
                SUCCESS);

    EXIT_ON_ERR(
        register_local_state_pcmc(SMARTT_LOCAL_STATE_FAST_COUNT, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_int_pcmc(
                    SMARTT_LOCAL_STATE_FAST_COUNT, 0, new_handle),
                SUCCESS);

    EXIT_ON_ERR(
        register_local_state_pcmc(SMARTT_LOCAL_STATE_FAST_ACTIVE, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_int_pcmc(
                    SMARTT_LOCAL_STATE_FAST_ACTIVE, 0, new_handle),
                SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(SIG_ACK, SIG_ACCUM_SUM, SMARTT_SIG_NUM_ACK,
                                     new_handle),
                SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(SMARTT_SIG_NUM_ACK, 1, new_handle),
        SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(SIG_RTO, SIG_ACCUM_SUM, SMARTT_SIG_NUM_RTO,
                                     new_handle),
                SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(SMARTT_SIG_NUM_RTO, 1, new_handle),
        SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(SIG_NACK, SIG_ACCUM_SUM,
                                     SMARTT_SIG_NUM_NACK, new_handle),
                SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(SMARTT_SIG_NUM_NACK, 1, new_handle),
        SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(SIG_ELAPSED_TIME, SIG_ACCUM_SUM,
                                     SMARTT_SIG_ELAPSED_TIME, new_handle),
                SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(SIG_RTT, SIG_ACCUM_LAST,
                                     SMARTT_SIG_LAST_RTT, new_handle),
                SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(SIG_ECN, SIG_ACCUM_SUM, SMARTT_SIG_NUM_ECN,
                                     new_handle),
                SUCCESS);

    EXIT_ON_ERR(
        register_control_pcmc(CTRL_CWND, SMARTT_CTRL_CWND_BYTES, new_handle),
        SUCCESS);
    EXIT_ON_ERR(register_control_initial_value_pcmc(
                    SMARTT_CTRL_CWND_BYTES, FABRIC_MAX_CWND, new_handle),
                SUCCESS);
    return SUCCESS;
}

int pcmc_init(const char *algo_name, device_t *dev_ctx,
              const char *reno_handler_path, handle_t *algo_handler) {

    handle_t new_handle;
    EXIT_ON_ERR(register_pcmc((void *)dev_ctx, 0, 0, 0, 0, &new_handle),
                SUCCESS);

    if (!strcmp(algo_name, "newreno")) {
        fprintf(stdout, "Algorithm requested: NewReno\n");
        EXIT_ON_ERR(tcp_pcmc_config(new_handle), SUCCESS);
    } else if (!strcmp(algo_name, "dctcp")) {
        fprintf(stdout, "Algorithm requested: DCTCP\n");
        EXIT_ON_ERR(tcp_pcmc_config(new_handle), SUCCESS);
        EXIT_ON_ERR(dctcp_pcmc_init(new_handle), SUCCESS);
    } else if (!strcmp(algo_name, "swift")) {
        fprintf(stdout, "Algorithm requested: Swift\n");
        EXIT_ON_ERR(swift_pcmc_init(new_handle), SUCCESS);
    } else if (!strcmp(algo_name, "dcqcn")) {
        fprintf(stdout, "Algorithm requested: DCQCN\n");
        EXIT_ON_ERR(dcqcn_pcmc_init(new_handle), SUCCESS);
    } else if (!strcmp(algo_name, "smartt")) {
        fprintf(stdout, "Algorithm requested: SMaRTT\n");
        EXIT_ON_ERR(smartt_pcmc_init(new_handle), SUCCESS);
    } else {
        fprintf(stderr, "Unknown algorithm name %s\n", algo_name);
        exit(EXIT_FAILURE);
    }

    char *compile_out;
    EXIT_ON_ERR(
        register_algorithm_pcmc(reno_handler_path, &compile_out, new_handle),
        SUCCESS);

    EXIT_ON_ERR(activate_pcmc(new_handle), SUCCESS);

    *algo_handler = new_handle;

    return 0;
}

int pcmc_destroy(handle_t algo_handler) {
    EXIT_ON_ERR(deactivate_pcmc(algo_handler), SUCCESS);
    EXIT_ON_ERR(deregister_pcmc(algo_handler), SUCCESS);
    return 0;
}

int main(int argc, char **argv) {
    EXIT_ON_ERR(argc != 5, 0);
    int num_flows = atoi(argv[1]);
    int test_duration = atoi(argv[2]);
    char *algo_name = argv[3];
    char *handler_path = argv[4];

    device_t *dev_ctx;
    EXIT_ON_ERR(device_init(NULL, &dev_ctx), SUCCESS);

    handle_t pcmc;
    EXIT_ON_ERR(pcmc_init(algo_name, dev_ctx, handler_path, &pcmc), 0);
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
    free(flows);

    EXIT_ON_ERR(pcmc_destroy(pcmc), 0);

    EXIT_ON_ERR(device_destroy(dev_ctx), SUCCESS);

    return 0;
}