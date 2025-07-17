#include <limits.h>

#include "algo_utils.h"
#include "pcm.h"
#include "dctcp.h"

int __tcp_pcmc_init(pcm_handle_t new_handle) {
    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_ACK, PCM_SIG_ACCUM_SUM, SIG_ACK,
                                     new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_invoke_trigger_pcmc(SIG_ACK, 1, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_RTO, PCM_SIG_ACCUM_SUM, SIG_RTO,
                                     new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_invoke_trigger_pcmc(SIG_RTO, 1, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_NACK, PCM_SIG_ACCUM_SUM, SIG_NACK,
                                     new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_invoke_trigger_pcmc(SIG_NACK, 1, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_control_pcmc(PCM_CTRL_CWND, CTRL_CWND, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(
        register_control_initial_value_pcmc(CTRL_CWND, 2048, new_handle),
        PCM_SUCCESS);

    EXIT_ON_ERR(register_local_state_pcmc(VAR_SSTHRESH, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(VAR_SSTHRESH, UINT_MAX,
                                                        new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_local_state_pcmc(VAR_TOT_ACKED, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(
        register_local_state_initial_value_pcmc(VAR_TOT_ACKED, 0, new_handle),
        PCM_SUCCESS);

    EXIT_ON_ERR(register_local_state_pcmc(VAR_IN_FAST_RECOV, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(VAR_IN_FAST_RECOV, 0,
                                                        new_handle),
                PCM_SUCCESS);

    return PCM_SUCCESS;
}

int __dctcp_pcmc_init(pcm_handle_t new_handle) {
    EXIT_ON_ERR(__tcp_pcmc_init(new_handle), PCM_SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_ECN, PCM_SIG_ACCUM_SUM, SIG_ECN,
                                     new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_local_state_pcmc(VAR_EPOCH_DELIVERED, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(VAR_EPOCH_DELIVERED, 0,
                                                        new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_local_state_pcmc(VAR_EPOCH_ECN_DELIVERED, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(VAR_EPOCH_ECN_DELIVERED,
                                                        0, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_local_state_pcmc(VAR_EPOCH_TO_DELIVER, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(VAR_EPOCH_TO_DELIVER,
                                                        2048, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_local_state_pcmc(VAR_ALPHA, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_float_pcmc(VAR_ALPHA, 1.0,
                                                              new_handle),
                PCM_SUCCESS);

    return PCM_SUCCESS;
}