#include "algo_utils.h"
#include "dcqcn.h"
#include "pcm.h"

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