#include "algo_utils.h"
#include "pcmh.h"

// per-flow trigger: sig_credit_target with threshold of 1
// threshold = sig_credit_target + 1 is adjusted dynamically
// every time cumulative_credit - credit_target >= max_cwnd
// shared trigger: timer of k * mtu / linkspeed that can be subscribed/unsubscribed

// trigger not faster then k*mtu/linkspeed seconds AND
// when cumulative_credit

#define CREDIT_QUOTA (CONST_K * CONST_MTU)

int algorithm_main() {
    pcm_uint cumulative_credit = get_control(CTRL_CUMULATIVE_CREDIT);
    pcm_uint credit_target = get_signal(SIG_CREDIT_TARGET);

    if (credit_target < cumulative_credit) {
        // source has unsatisfied demand
        cumulative_credit = MIN(credit_target, cumulative_credit + CREDIT_QUOTA);
    } else {
        // source's deman is satisfied but we allow it to speculate
        cumulative_credit += CREDIT_QUOTA;
    }

    set_control(CTRL_CUMULATIVE_CREDIT, cumulative_credit);

    if (cumulative_credit - credit_target >= MAX_CWND) {
        // source was granted with speculative budget,
        // therefore this flow can be unsubscribed from the shared timer for now
        set_signal(SIG_CREDIT_TIMER, PCM_SIG_NO_TRIGGER);
        // serve this source again as soon as starts to consume credit
        set_trigger_threshold(SIG_CREDIT_TARGET, get_signal(SIG_CREDIT_TARGET) + 1);
    } else if (get_signal_trigger_mask() & SIG_CREDIT_TARGET) {
        // source still needs credit and subscribes to the shared timer
        set_trigger_threshold(SIG_CREDIT_TIMER, PCM_SIG_REARM);
    }

    return 0;
}