#include "dctcp_v2.h"
#include "tcp_utils.h"
#include <assert.h>

#define DCTCP_SSTHRESH(cur_cwnd) (MAX(cur_cwnd * (1.0 - get_var_float(ALPHA) / 2.0), 2U))
FAST_RECOVERY_DEFINE(dctcp, DCTCP_SSTHRESH);

static PCM_FORCE_INLINE void dctcp_alpha_update(ALGO_CTX_ARGS, pcm_uint num_acks, pcm_uint num_ecn) {
    pcm_uint delivered = get_var_uint(EPOCH_DELIVERED) + num_acks;
    pcm_uint delivered_ecn = get_var_uint(EPOCH_ECN_DELIVERED) + num_ecn;

    if (delivered >= get_var_uint(EPOCH_TO_DELIVER)) {
        pcm_float F = (pcm_float)delivered_ecn / (pcm_float)delivered;
        set_var_float(ALPHA, (1 - GAMMA) * get_var_float(ALPHA) + GAMMA * F);
        set_var_uint(EPOCH_DELIVERED, 0);
        set_var_uint(EPOCH_ECN_DELIVERED, 0);
    } else {
        set_var_uint(EPOCH_DELIVERED, delivered);
        set_var_uint(EPOCH_ECN_DELIVERED, delivered_ecn);
    }
}

int algorithm_main() {
    pcm_uint cur_cwnd = get_control(CWND) / MSS;

    // Calculate Deltas
    pcm_uint num_acks = get_signal(ACK) - get_var_uint(PREV_ACK);
    pcm_uint num_nacks = get_signal(NACK) - get_var_uint(PREV_NACK);
    pcm_uint num_rtos = get_signal(RTO) - get_var_uint(PREV_RTO);
    pcm_uint num_ecn = get_signal(ECN) - get_var_uint(PREV_ECN);

    // Update Cache
    set_var_uint(PREV_ACK, get_signal(ACK));
    set_var_uint(PREV_NACK, get_signal(NACK));
    set_var_uint(PREV_RTO, get_signal(RTO));
    set_var_uint(PREV_ECN, get_signal(ECN));

    pcm_uint acks_to_consume = 0;

    if (num_nacks > 0) {
        dctcp_fast_recovery(ALGO_CTX_PASS, &cur_cwnd);
        goto save_cwnd_and_exit;
    }

    if (num_rtos > 0) {
        tcp_timeout_recovery(ALGO_CTX_PASS, &cur_cwnd);
        goto save_cwnd_and_exit;
    }

    acks_to_consume = num_acks;

    if (get_var(IN_FAST_RECOV) && acks_to_consume > 0)
        tcp_fast_recovery_exit(ALGO_CTX_PASS, &cur_cwnd, &acks_to_consume);

    if (!get_var(IN_FAST_RECOV) && acks_to_consume > 0) {
        if (cur_cwnd < get_var(SSTHRESH)) {
            tcp_slow_start(ALGO_CTX_PASS, &cur_cwnd, &acks_to_consume);
        }
        if (acks_to_consume) {
            tcp_cong_avoid(ALGO_CTX_PASS, &cur_cwnd, &acks_to_consume);
        }
    }

save_cwnd_and_exit:
    set_control(CWND, cur_cwnd * MSS);
    dctcp_alpha_update(ALGO_CTX_PASS, num_acks, num_ecn);

    return PCM_SUCCESS;
}
