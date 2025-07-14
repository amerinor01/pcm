#include "algo_utils.h"
#include "dcqcn.h"
#include "pcm.h"

int dcqcn_pcmc_init(handle_t new_handle) {
    EXIT_ON_ERR(register_signal_pcmc(SIG_ECN, SIG_ACCUM_SUM, DCQCN_SIG_IDX_ECN,
                                     new_handle),
                SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(DCQCN_SIG_IDX_ECN, 1, new_handle),
        SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(SIG_ELAPSED_TIME, SIG_ACCUM_SUM,
                                     DCQCN_SIG_IDX_ALPHA_TIMER, new_handle),
                SUCCESS);
    EXIT_ON_ERR(register_signal_invoke_trigger_pcmc(DCQCN_SIG_IDX_ALPHA_TIMER,
                                                    DEFAULT_DCQCN_ALPHA_TIMER,
                                                    new_handle),
                SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(SIG_ELAPSED_TIME, SIG_ACCUM_SUM,
                                     DCQCN_SIG_IDX_RATE_INCREASE_TIMER,
                                     new_handle),
                SUCCESS);
    EXIT_ON_ERR(register_signal_invoke_trigger_pcmc(
                    DCQCN_SIG_IDX_RATE_INCREASE_TIMER,
                    DEFAULT_DCQCN_RATE_INCREASE_TIMER, new_handle),
                SUCCESS);

    EXIT_ON_ERR(register_signal_pcmc(SIG_DATA_TX, SIG_ACCUM_SUM,
                                     DCQCN_SIG_IDX_TX_BURST, new_handle),
                SUCCESS);
    EXIT_ON_ERR(register_signal_invoke_trigger_pcmc(DCQCN_SIG_IDX_TX_BURST,
                                                    DEFAULT_DCQCN_BYTE_COUNTER,
                                                    new_handle),
                SUCCESS);

    EXIT_ON_ERR(
        register_local_state_pcmc(DCQCN_LOCAL_STATE_IDX_ALPHA, new_handle),
        SUCCESS);
    EXIT_ON_ERR(
        register_local_state_initial_value_float_pcmc(
            DCQCN_LOCAL_STATE_IDX_ALPHA, DEFAULT_DCQCN_ALPHA_INIT, new_handle),
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

    struct dcqcn_state_snapshot state = {0};
    state.consts.brtt = FABRIC_BRTT;
    state.consts.rai = DEFAULT_DCQCN_RAI;
    state.consts.rhai = DEFAULT_DCQCN_RHAI;
    state.consts.fr_steps = DEFAULT_DCQCN_FR_STEPS;
    state.consts.gamma = DEFAULT_DCQCN_GAMMA;

    EXIT_ON_ERR(register_constant_pcmc(DCQCN_CONST_BRTT, new_handle), SUCCESS);
    EXIT_ON_ERR(register_constant_value_uint_pcmc(
                    DCQCN_CONST_BRTT, state.consts.brtt, new_handle),
                SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(DCQCN_CONST_RAI, new_handle), SUCCESS);
    EXIT_ON_ERR(register_constant_value_uint_pcmc(DCQCN_CONST_RAI,
                                                  state.consts.rai, new_handle),
                SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(DCQCN_CONST_RHAI, new_handle), SUCCESS);
    EXIT_ON_ERR(register_constant_value_uint_pcmc(
                    DCQCN_CONST_RHAI, state.consts.rhai, new_handle),
                SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(DCQCN_CONST_FR_STEPS, new_handle),
                SUCCESS);
    EXIT_ON_ERR(register_constant_value_uint_pcmc(
                    DCQCN_CONST_FR_STEPS, state.consts.fr_steps, new_handle),
                SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(DCQCN_CONST_GAMMA, new_handle), SUCCESS);
    EXIT_ON_ERR(register_constant_value_float_pcmc(
                    DCQCN_CONST_GAMMA, state.consts.gamma, new_handle),
                SUCCESS);

    return SUCCESS;
}