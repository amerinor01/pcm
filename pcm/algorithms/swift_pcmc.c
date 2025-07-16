#include "algo_utils.h"
#include "pcm.h"
#include "swift.h"

int swift_pcmc_init(pcm_handle_t new_handle) {
    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_ACK, PCM_SIG_ACCUM_SUM, SWIFT_SIG_IDX_ACK,
                                     new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(SWIFT_SIG_IDX_ACK, 1, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_RTO, PCM_SIG_ACCUM_SUM, SWIFT_SIG_IDX_RTO,
                                     new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(SWIFT_SIG_IDX_RTO, 1, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_NACK, PCM_SIG_ACCUM_SUM,
                                     SWIFT_SIG_IDX_NACK, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(
        register_signal_invoke_trigger_pcmc(SWIFT_SIG_IDX_NACK, 1, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_RTT, PCM_SIG_ACCUM_LAST, SWIFT_SIG_IDX_RTT,
                                     new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_signal_pcmc(PCM_SIG_ELAPSED_TIME, PCM_SIG_ACCUM_SUM,
                                     SIWFT_SIG_IDX_ELAPSED_TIME, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(
        register_control_pcmc(PCM_CTRL_CWND, SWIFT_CTRL_IDX_CWND, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(register_control_initial_value_pcmc(
                    SWIFT_CTRL_IDX_CWND, FABRIC_MIN_CWND, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(
        register_local_state_pcmc(SWIFT_LOCAL_STATE_IDX_ACKED, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(
                    SWIFT_LOCAL_STATE_IDX_ACKED, 0, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_local_state_pcmc(SWIFT_LOCAL_STATE_IDX_T_LAST_DECREASE,
                                          new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(
                    SWIFT_LOCAL_STATE_IDX_T_LAST_DECREASE, 0, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_local_state_pcmc(SWIFT_LOCAL_STATE_IDX_RETRANSMIT_CNT,
                                          new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(
                    SWIFT_LOCAL_STATE_IDX_RETRANSMIT_CNT, 0, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(
        register_local_state_pcmc(SWIFT_LOCAL_STATE_IDX_RTT_ESTIM, new_handle),
        PCM_SUCCESS);
    EXIT_ON_ERR(register_local_state_initial_value_pcmc(
                    SWIFT_LOCAL_STATE_IDX_RTT_ESTIM, FABRIC_BRTT, new_handle),
                PCM_SUCCESS);

    struct swift_state_snapshot state = {0};
    state.consts.brtt = FABRIC_BRTT;
    state.consts.bdp = FABRIC_BDP;
    state.consts.hop_count = FABRIC_HOP_COUNT;
    state.consts.mss = FABRIC_LINK_MSS;
    state.consts.rtx_thresh = 4;
    state.consts.h = (pcm_float)state.consts.brtt / 6.55;
    state.consts.fs_range = 5 * state.consts.brtt;
    state.consts.rtx_thresh = 5;
    state.consts.max_mdf = 0.5;
    state.consts.fs_alpha =
        state.consts.fs_range /
        ((1.0 / sqrt(0.1) - (1.0 / sqrt(state.consts.bdp / state.consts.mss))));
    state.consts.fs_beta =
        -(state.consts.fs_alpha / sqrt(state.consts.bdp / state.consts.mss));
    state.consts.beta = 0.8;
    state.consts.ai = 1.0;

    EXIT_ON_ERR(register_constant_pcmc(SWIFT_CONST_BRTT, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_uint_pcmc(
                    SWIFT_CONST_BRTT, state.consts.brtt, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(SWIFT_CONST_BDP, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_uint_pcmc(SWIFT_CONST_BDP,
                                                  state.consts.bdp, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(SWIFT_CONST_MSS, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_uint_pcmc(SWIFT_CONST_MSS,
                                                  state.consts.mss, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(SWIFT_CONST_HOP_COUNT, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_uint_pcmc(
                    SWIFT_CONST_HOP_COUNT, state.consts.hop_count, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(SWIFT_CONST_RTX_THRESH, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_uint_pcmc(SWIFT_CONST_RTX_THRESH,
                                                  state.consts.rtx_thresh,
                                                  new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(SWIFT_CONST_AI, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_float_pcmc(SWIFT_CONST_AI,
                                                   state.consts.ai, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(SWIFT_CONST_MAX_MDF, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_float_pcmc(
                    SWIFT_CONST_MAX_MDF, state.consts.max_mdf, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(SWIFT_CONST_FS_RANGE, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_float_pcmc(
                    SWIFT_CONST_FS_RANGE, state.consts.fs_range, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(SWIFT_CONST_FS_ALPHA, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_float_pcmc(
                    SWIFT_CONST_FS_ALPHA, state.consts.fs_alpha, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(SWIFT_CONST_FS_BETA, new_handle),
                PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_float_pcmc(
                    SWIFT_CONST_FS_BETA, state.consts.fs_beta, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(SWIFT_CONST_BETA, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_float_pcmc(
                    SWIFT_CONST_BETA, state.consts.beta, new_handle),
                PCM_SUCCESS);

    EXIT_ON_ERR(register_constant_pcmc(SWIFT_CONST_H, new_handle), PCM_SUCCESS);
    EXIT_ON_ERR(register_constant_value_float_pcmc(SWIFT_CONST_H,
                                                   state.consts.h, new_handle),
                PCM_SUCCESS);
    return PCM_SUCCESS;
}