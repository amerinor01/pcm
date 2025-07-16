#include "algo_utils.h"
#include "pcm.h"
#include "tcp.h"

int tcp_pcmc_init(pcm_handle_t new_handle) {
    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_ACK, PCM_SIG_ACCUM_SUM, TCP_SIG_IDX_ACK,
                                     new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(TCP_SIG_IDX_ACK, 1, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_RTO, PCM_SIG_ACCUM_SUM, TCP_SIG_IDX_RTO,
                                     new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(TCP_SIG_IDX_RTO, 1, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_NACK, PCM_SIG_ACCUM_SUM, TCP_SIG_IDX_NACK,
                                     new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(TCP_SIG_IDX_NACK, 1, new_handle),
        PCM_SUCCESS);

    EXIT_ON_ERR(register_control_pcmc(PCM_CTRL_CWND, TCP_CTRL_IDX_CWND, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_control_initial_value_pcmc(
                    TCP_CTRL_IDX_CWND, FABRIC_MIN_CWND, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(
        register_local_state_pcmc(TCP_LOCAL_STATE_IDX_SSTHRESH, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(
                    TCP_LOCAL_STATE_IDX_SSTHRESH, DEFAULT_TCP_SSTHRESH_INIT,
                    new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(
        register_local_state_pcmc(TCP_LOCAL_STATE_IDX_ACKED, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(
                    TCP_LOCAL_STATE_IDX_ACKED, 0, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_local_state_pcmc(TCP_LOCAL_STATE_IDX_IN_FAST_RECOV,
                                          new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(
                    TCP_LOCAL_STATE_IDX_IN_FAST_RECOV, 0, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(TCP_CONST_MSS, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_uint_pcmc(TCP_CONST_MSS,
                                                  FABRIC_LINK_MSS, new_handle),
                PCM_SUCCESS);

    return PCM_SUCCESS;
}

int dctcp_pcmc_init(pcm_handle_t new_handle) {
    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_ECN, PCM_SIG_ACCUM_SUM, TCP_SIG_IDX_ECN,
                                     new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_local_state_pcmc(TCP_LOCAL_STATE_IDX_EPOCH_DELIVERED,
                                          new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(
                    TCP_LOCAL_STATE_IDX_EPOCH_DELIVERED, 0, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_local_state_pcmc(
                    TCP_LOCAL_STATE_IDX_EPOCH_ECN_DELIVERED, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(
                    TCP_LOCAL_STATE_IDX_EPOCH_ECN_DELIVERED, 0, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_local_state_pcmc(TCP_LOCAL_STATE_IDX_EPOCH_TO_DELIVER,
                                          new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(
        register_local_state_initial_value_pcmc(
            TCP_LOCAL_STATE_IDX_EPOCH_TO_DELIVER, FABRIC_MIN_CWND, new_handle),
        PCM_SUCCESS);

    EXIT_ON_ERR(
        register_local_state_pcmc(TCP_LOCAL_STATE_IDX_ALPHA, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(
        register_local_state_initial_value_float_pcmc(
            TCP_LOCAL_STATE_IDX_ALPHA, DEFAULT_DCTCP_MAX_ALPHA, new_handle),
        PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(TCP_CONST_GAMMA, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_float_pcmc(
                    TCP_CONST_GAMMA, DEFAULT_DCTCP_GAMMA, new_handle),
                PCM_SUCCESS);

    return PCM_SUCCESS;
}