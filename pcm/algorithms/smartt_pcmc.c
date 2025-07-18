#include "algo_utils.h"
#include "pcm.h"
#include "smartt.h"

int __smartt_pcmc_init(pcm_handle_t new_handle) {
    EXIT_ON_ERR(register_var_pcmc(VAR_ACKED_BYTES, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_var_initial_value_int_pcmc(VAR_ACKED_BYTES, 0,
                                                            new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_var_pcmc(VAR_BYTES_IGNORED, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_var_initial_value_int_pcmc(VAR_BYTES_IGNORED,
                                                            0, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_var_pcmc(VAR_BYTES_TO_IGNORE, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_var_initial_value_int_pcmc(VAR_BYTES_TO_IGNORE,
                                                            0, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_var_pcmc(VAR_TRIGGER_QA, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_var_initial_value_int_pcmc(VAR_TRIGGER_QA, 0,
                                                            new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_var_pcmc(VAR_QA_DEADLINE, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_var_initial_value_int_pcmc(VAR_QA_DEADLINE, 0,
                                                            new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_var_pcmc(VAR_FAST_COUNT, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_var_initial_value_int_pcmc(VAR_FAST_COUNT, 0,
                                                            new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_var_pcmc(VAR_FAST_ACTIVE, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_var_initial_value_int_pcmc(VAR_FAST_ACTIVE, 0,
                                                            new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_ACK, PCM_SIG_ACCUM_SUM,
                                     SIG_NUM_ACK, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_invoke_trigger_pcmc(SIG_NUM_ACK, 1, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_RTO, PCM_SIG_ACCUM_SUM,
                                     SIG_NUM_RTO, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_invoke_trigger_pcmc(SIG_NUM_RTO, 1, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_NACK, PCM_SIG_ACCUM_SUM,
                                     SIG_NUM_NACK, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(SIG_NUM_NACK, 1, new_handle),
        PCM_SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_ELAPSED_TIME, PCM_SIG_ACCUM_SUM,
                                     SIG_ELAPSED_TIME, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_RTT, PCM_SIG_ACCUM_LAST,
                                     SIG_RTT_SAMPLE, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_ECN, PCM_SIG_ACCUM_SUM,
                                     SIG_NUM_ECN, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(
        register_control_pcmc(PCM_CTRL_CWND, CTRL_CWND_BYTES, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(register_control_initial_value_pcmc(
                    CTRL_CWND_BYTES, FABRIC_MAX_CWND, new_handle),
                PCM_SUCCESS);

    return PCM_SUCCESS;
}