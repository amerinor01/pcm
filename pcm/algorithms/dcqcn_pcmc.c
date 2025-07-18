#include "algo_utils.h"
#include "dcqcn.h"
#include "pcm.h"

int __dcqcn_pcmc_init(pcm_handle_t new_handle) {
    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_ECN, PCM_SIG_ACCUM_SUM, SIG_ECN,
                                     new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(SIG_ECN, 1, new_handle),
        PCM_SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_ELAPSED_TIME, PCM_SIG_ACCUM_SUM,
                                     SIG_ALPHA_TIMER, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_invoke_trigger_pcmc(SIG_ALPHA_TIMER,
                                                    DEFAULT_DCQCN_ALPHA_TIMER,
                                                    new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_ELAPSED_TIME, PCM_SIG_ACCUM_SUM,
                                     SIG_RATE_INCREASE_TIMER,
                                     new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_invoke_trigger_pcmc(
                    SIG_RATE_INCREASE_TIMER,
                    DEFAULT_DCQCN_RATE_INCREASE_TIMER, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_DATA_TX, PCM_SIG_ACCUM_SUM,
                                     SIG_TX_BURST, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_invoke_trigger_pcmc(SIG_TX_BURST,
                                                    DEFAULT_DCQCN_BYTE_COUNTER,
                                                    new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(
        register_var_pcmc(VAR_ALPHA, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(
        register_var_initial_value_float_pcmc(
            VAR_ALPHA, DEFAULT_DCQCN_ALPHA_INIT, new_handle),
        PCM_SUCCESS);

    EXIT_ON_ERR(
        register_var_pcmc(VAR_CUR_RATE, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(
        register_var_initial_value_float_pcmc(
            VAR_CUR_RATE, FABRIC_LINK_RATE_GBPS, new_handle),
        PCM_SUCCESS);

    EXIT_ON_ERR(register_var_pcmc(VAR_TGT_RATE,
                                          new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_var_initial_value_float_pcmc(
                    VAR_TGT_RATE, FABRIC_LINK_RATE_GBPS,
                    new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_var_pcmc(
                    VAR_RATE_INCREASE_EVTS, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_var_initial_value_int_pcmc(
                    VAR_RATE_INCREASE_EVTS, 0, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_var_pcmc(
                    VAR_BYTE_COUNTER_EVTS, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_var_initial_value_int_pcmc(
                    VAR_BYTE_COUNTER_EVTS, 0, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(
        register_control_pcmc(PCM_CTRL_CWND, CTRL_CWND, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(register_control_initial_value_pcmc(
                    CTRL_CWND, FABRIC_MAX_CWND, new_handle),
                PCM_SUCCESS);

    return PCM_SUCCESS;
}