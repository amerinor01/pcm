#include "algo_utils.h"
#include "pcm.h"
#include "swift.h"

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