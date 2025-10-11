#include "bitmap_lb_v2.h"
#include "pcmh.h"
#include "stdbool.h"
#include "stdlib.h"

// Bitmap manipulation helpers
#define BITS_PER_UINT (sizeof(pcm_uint) * 8)
#define BITMAP_ARRAY_SIZE ((EVS_SIZE + BITS_PER_UINT - 1) / BITS_PER_UINT)

static PCM_FORCE_INLINE void set_bitmap_entry(ALGO_CTX_ARGS, pcm_uint bit_idx, pcm_uint bit) {
    // pcm_uint word_idx = bit_idx / BITS_PER_UINT;
    pcm_uint bit_offset = bit_idx % BITS_PER_UINT;
    pcm_uint mask = 1ULL << bit_offset;

    // word = get_array_uint(EVS_BITMAP, word_idx);
    pcm_uint word = get_var_uint(VAR_EVS_BITMAP);
    word |= mask; // Set bit

    // set_array_uint(EVS_BITMAP, word_idx, word);
    set_var_uint(VAR_EVS_BITMAP, word);
}

static PCM_FORCE_INLINE pcm_uint get_bitmap_entry(ALGO_CTX_ARGS, pcm_uint bit_idx) {
    // pcm_uint word_idx = bit_idx / BITS_PER_UINT;
    pcm_uint bit_offset = bit_idx % BITS_PER_UINT;
    pcm_uint mask = 1ULL << bit_offset;

    // word = get_array_uint(EVS_BITMAP, word_idx);
    pcm_uint word = get_var_uint(VAR_EVS_BITMAP);
    return word & mask;
}

// for now we assume that handler keeps up with the arrival rate of TX/ACK packets
// we also don't handle path failure (which should be exposed as a datapath signal)
// and, therefore, don't support implement freezing mode

int algorithm_main() {
    pcm_uint trigger_mask = get_signal_trigger_mask();

    // RX path
    if (trigger_mask & SIG_NUM_ACK) {
        set_bitmap_entry(ALGO_CTX_PASS, get_signal(SIG_ACK_EV), 1);
        set_signal(SIG_NUM_ACK, 0);
    }
    if (trigger_mask & SIG_NUM_ECN) {
        set_bitmap_entry(ALGO_CTX_PASS, get_signal(SIG_ECN_EV), 1);
        set_signal(SIG_NUM_ECN, 0);
    }
    if (trigger_mask & SIG_NUM_NACK) {
        set_bitmap_entry(ALGO_CTX_PASS, get_signal(SIG_NACK_EV), 1);
        set_signal(SIG_NUM_NACK, 0);
    }

    // TX path
    if (trigger_mask & SIG_TX_BACKLOG_SIZE) {
        pcm_uint i = 0;
        pcm_uint ev = rand() % EVS_SIZE;
        bool is_marked = get_bitmap_entry(ALGO_CTX_PASS, ev);
        while (is_marked && i++ < EVS_SIZE) {
            set_bitmap_entry(ALGO_CTX_PASS, ev, 0);
            ev = (ev + 1) % EVS_SIZE;
            is_marked = get_bitmap_entry(ALGO_CTX_PASS, ev);
        }
        set_control(CTRL_NEXT_PKT_EV, ev);
        update_signal(SIG_TX_BACKLOG_SIZE, -1);
    }

    return PCM_SUCCESS;
}