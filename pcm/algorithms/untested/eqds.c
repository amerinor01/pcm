#include "algo_utils.h"
#include "pcmh.h"

#define CTRL_CUMULATIVE_CREDIT 0
#define SIG_CREDIT_TARGET 1
#define CONST_K 1
#define CONST_MTU 1
#define MAX_CWND 10
#define SIG_CREDIT_TIMER 10
#define PCM_SIG_TRIGGER_DISABLE 10


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
        set_trigger_threshold(SIG_CREDIT_TIMER, PCM_SIG_TRIGGER_DISABLE);
        // serve this source again as soon as it starts to consume credit
        set_trigger_threshold(SIG_CREDIT_TARGET, get_signal(SIG_CREDIT_TARGET) + 1);
    } else if (get_signal_trigger_mask() & SIG_CREDIT_TARGET) {
        // source still needs credit and keeps shared timer subscription
        set_trigger_threshold(SIG_CREDIT_TIMER, PCM_SIG_REARM);
    }

    return 0;
}

