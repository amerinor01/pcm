#include "algo_utils.h"
#include "pcm.h"
#include "smartt.h"

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