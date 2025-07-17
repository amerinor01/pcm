#include "algo_utils.h"
#include "pcm.h"
#include "smartt.h"

int __smartt_pcmc_init(pcm_handle_t new_handle) {
    EXIT_ON_ERR(
        register_local_state_pcmc(SMARTT_LOCAL_STATE_ACKED_BYTES, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_int_pcmc(
                    SMARTT_LOCAL_STATE_ACKED_BYTES, 0, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(
        register_local_state_pcmc(SMARTT_LOCAL_STATE_BYTES_IGNORED, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_int_pcmc(
                    SMARTT_LOCAL_STATE_BYTES_IGNORED, 0, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_local_state_pcmc(SMARTT_LOCAL_STATE_BYTES_TO_IGNORE,
                                          new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_int_pcmc(
                    SMARTT_LOCAL_STATE_BYTES_TO_IGNORE, 0, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(
        register_local_state_pcmc(SMARTT_LOCAL_STATE_TRIGGER_QA, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_int_pcmc(
                    SMARTT_LOCAL_STATE_TRIGGER_QA, 0, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(
        register_local_state_pcmc(SMARTT_LOCAL_STATE_QA_DEADLINE, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_int_pcmc(
                    SMARTT_LOCAL_STATE_QA_DEADLINE, 0, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(
        register_local_state_pcmc(SMARTT_LOCAL_STATE_FAST_COUNT, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_int_pcmc(
                    SMARTT_LOCAL_STATE_FAST_COUNT, 0, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(
        register_local_state_pcmc(SMARTT_LOCAL_STATE_FAST_ACTIVE, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_int_pcmc(
                    SMARTT_LOCAL_STATE_FAST_ACTIVE, 0, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_ACK, PCM_SIG_ACCUM_SUM, SMARTT_SIG_NUM_ACK,
                                     new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(SMARTT_SIG_NUM_ACK, 1, new_handle),
        PCM_SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_RTO, PCM_SIG_ACCUM_SUM, SMARTT_SIG_NUM_RTO,
                                     new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(SMARTT_SIG_NUM_RTO, 1, new_handle),
        PCM_SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_NACK, PCM_SIG_ACCUM_SUM,
                                     SMARTT_SIG_NUM_NACK, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(SMARTT_SIG_NUM_NACK, 1, new_handle),
        PCM_SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_ELAPSED_TIME, PCM_SIG_ACCUM_SUM,
                                     SMARTT_SIG_ELAPSED_TIME, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_RTT, PCM_SIG_ACCUM_LAST,
                                     SMARTT_SIG_LAST_RTT, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_ECN, PCM_SIG_ACCUM_SUM, SMARTT_SIG_NUM_ECN,
                                     new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(
        register_control_pcmc(PCM_CTRL_CWND, SMARTT_CTRL_CWND_BYTES, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(register_control_initial_value_pcmc(
                    SMARTT_CTRL_CWND_BYTES, FABRIC_MAX_CWND, new_handle),
                PCM_SUCCESS);

    struct smartt_state_snapshot state = {0};

    state.consts.bdp = FABRIC_BDP;
    state.consts.brtt = FABRIC_BRTT;
    state.consts.trtt = FABRIC_TRTT;
    state.consts.mss = FABRIC_LINK_MSS;
    state.consts.x_gain = 2.0;
    state.consts.y_gain = 2.5;
    state.consts.z_gain = 2;
    state.consts.w_gain = 0.8;
    state.consts.reaction_delay = 1.0;
    state.consts.qa_scaling = 1;

    EXIT_ON_ERR(register_constant_pcmc(SMARTT_CONST_BDP, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_uint_pcmc(SMARTT_CONST_BDP,
                                                  state.consts.bdp, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(SMARTT_CONST_BRTT, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_uint_pcmc(
                    SMARTT_CONST_BRTT, state.consts.brtt, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(SMARTT_CONST_TRTT, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_uint_pcmc(
                    SMARTT_CONST_TRTT, state.consts.trtt, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(SMARTT_CONST_MSS, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_uint_pcmc(SMARTT_CONST_MSS,
                                                  state.consts.mss, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(SMARTT_CONST_X_GAIN, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_float_pcmc(
                    SMARTT_CONST_X_GAIN, state.consts.x_gain, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(SMARTT_CONST_Y_GAIN, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_float_pcmc(
                    SMARTT_CONST_Y_GAIN, state.consts.y_gain, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(SMARTT_CONST_Z_GAIN, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_float_pcmc(
                    SMARTT_CONST_Z_GAIN, state.consts.z_gain, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(SMARTT_CONST_W_GAIN, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_float_pcmc(
                    SMARTT_CONST_W_GAIN, state.consts.w_gain, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(SMARTT_CONST_REACTION_DELAY, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_float_pcmc(SMARTT_CONST_REACTION_DELAY,
                                                   state.consts.reaction_delay,
                                                   new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(SMARTT_CONST_QA_SCALING, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_float_pcmc(SMARTT_CONST_QA_SCALING,
                                                   state.consts.qa_scaling,
                                                   new_handle),
                PCM_SUCCESS);
    return PCM_SUCCESS;
}