#include "pcmh.h"
#include "stdbool.h"

// signals
#define NUM_ECN 0 // trigger w threshold=1, sum
#define NUM_SENDS 1 // trigger w threshold=1, sum
#define ECN_ACK_EV 2 // last

// controls 
#define PKT_EV 0

// variables
#define EVC_HEAD_IDX 0
#define EVC_NUM_VALID_EVS 1
#define EV_EXPLORE_COUNTER 2

#define BITMAP_CONGESTED_PATHS 0


// for now we assume that handler keeps up with the arrival rate of TX/ACK packets
// we also don't handle path failure (which should be exposed as a datapath signal)
// and, therefore, don't support implement freezing mode
int algorithm_main() {
    pcm_uint trigger_mask = get_signal_trigger_mask();

    if (trigger_mask & NUM_ECN) {
        set_bitmap_entry(BITMAP_CONGESTED_PATHS, get_signal(ECN_ACK_EV), 1);
    }

    if (trigger_mask & NUM_SENDS) {
        pcm_uint packet_ev = 0;
        bool found = false;
        while (!found) {
            packet_ev = rand() % get_bitmap_size(BITMAP_CONGESTED_PATHS);
            if (!get_bitmap_entry(BITMAP_CONGESTED_PATHS, packet_ev)) {
                found = true;
            } else {
                set_bitmap_entry(BITMAP_CONGESTED_PATHS, packet_ev, 0);
            }
        }
        set_control(PKT_EV, packet_ev);
    }

    return PCM_SUCCESS;
}