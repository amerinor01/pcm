#include "tcp_utils.h"

PCM_FORCE_INLINE void tcp_fast_recovery_exit(ALGO_CTX_ARGS, pcm_uint *cur_cwnd,
                                             pcm_uint *num_acks) {
    *cur_cwnd = get_var_uint(SSTHRESH);
    (*num_acks)--;
    set_var_uint(IN_FAST_RECOV, 0);
}

PCM_FORCE_INLINE void tcp_timeout_recovery(ALGO_CTX_ARGS, pcm_uint *cur_cwnd) {
    set_var_uint(SSTHRESH, TCP_RTO_RECOVERY_SSTHRESH(*cur_cwnd));
    *cur_cwnd = 1;
    set_var_uint(IN_FAST_RECOV, 0);
}

PCM_FORCE_INLINE void tcp_slow_start(ALGO_CTX_ARGS, pcm_uint *cur_cwnd, pcm_uint *acks_to_consume) {
    pcm_uint to_ssthresh = MIN(*acks_to_consume, get_var_uint(SSTHRESH) - *cur_cwnd);
    *cur_cwnd += to_ssthresh;
    *acks_to_consume -= to_ssthresh;
}

PCM_FORCE_INLINE void tcp_cong_avoid(ALGO_CTX_ARGS, pcm_uint *cur_cwnd, pcm_uint *acks_to_consume) {
    pcm_uint tot_acked = get_var_uint(TOT_ACKED);

    if (tot_acked >= *cur_cwnd) {
        /* If credits accumulated at a higher w, apply them gently now. */
        tot_acked = 0;
        *cur_cwnd += 1;
    }
    tot_acked += *acks_to_consume;
    if (tot_acked >= *cur_cwnd) {
        pcm_uint delta = tot_acked / *cur_cwnd;
        tot_acked -= delta * (*cur_cwnd);
        *cur_cwnd += delta;
    }
    *acks_to_consume = 0;

    set_var_uint(TOT_ACKED, tot_acked);
}