#include "algo_utils.h"
#include "pcm.h"
#include "swift.h"

int __swift_pcmc_init(pcm_handle_t new_handle)
{
    /* Signals */
    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_NACK, PCM_SIG_ACCUM_SUM, SIG_NACK, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_invoke_trigger_pcmc(SIG_NACK, 1, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_RTO, PCM_SIG_ACCUM_SUM, SIG_RTO, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_invoke_trigger_pcmc(SIG_RTO, 1, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_ACK, PCM_SIG_ACCUM_SUM, SIG_ACK, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_invoke_trigger_pcmc(SIG_ACK, 1, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_RTT, PCM_SIG_ACCUM_LAST, SIG_RTT, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_ELAPSED_TIME, PCM_SIG_ACCUM_SUM, SIG_ELAPSED_TIME, new_handle), PCM_SUCCESS);

    /* Controls */
    EXIT_ON_ERR(register_control_pcmc(PCM_CTRL_CWND, CTRL_CWND, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_control_initial_value_pcmc(CTRL_CWND, 2048, new_handle), PCM_SUCCESS);

    /* Variables */
    EXIT_ON_ERR(register_local_state_pcmc(VAR_ACKED, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(VAR_ACKED, 0, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_pcmc(VAR_T_LAST_DECREASE, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(VAR_T_LAST_DECREASE, 0, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_pcmc(VAR_RETX_CNT, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(VAR_RETX_CNT, 0, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_pcmc(VAR_RTT_ESTIM, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(VAR_RTT_ESTIM, 5058000, new_handle), PCM_SUCCESS);

    return PCM_SUCCESS;
}