#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "app_traffic_gen.h"
#include "network.h"
#include "pcm.h"

// Flow thread: emulate constant bandwidth, drops, NACKs, RTOs
void *app_flow_traffic_gen_fn(void *arg) {
    flow_t *flow = arg;

    if (flow_progress_state_get(flow) != FLOW_THREAD_INIT) {
        flow_error_status_set(flow, ERROR);
        goto flow_thread_join;
    }

    fprintf(stdout, "[flow=%p,] traffic generation started\n", flow);

    unsigned int rnd = (unsigned int)time(NULL);
    const int pkts_per_ms = (TGEN_BANDWIDTH_BPS / 8 / 1000) / TGEN_PACKET_SIZE;

    // Initialize time related signals of flow before traffic generation
    // starts
    if (flow_time_init(flow) != SUCCESS) {
        flow_error_status_set(flow, ERROR);
        flow_progress_state_set(flow, FLOW_THREAD_RUNNING);
        goto flow_thread_join;
    }

    // At that point flow is not yet added to the scheduler, so
    // all flow triggers need to be manually armed
    flow_signal_triggers_rearm(flow);

    // Signal that flow is ready to be added to the scheduler
    flow_progress_state_set(flow, FLOW_THREAD_RUNNING);

    while (flow_progress_state_get(flow) == FLOW_THREAD_RUNNING) {
        int cwnd = flow_cwnd_get(flow);
        if (cwnd == 0) {
            usleep(TGEN_THREAD_SLEEP_TIME_US);
            continue;
        }
        int to_send = cwnd < pkts_per_ms ? cwnd : pkts_per_ms;
        flow_signals_update(flow, SIG_DATA_TX, to_send * TGEN_MSS);
        double roll = (double)rand_r(&rnd) / RAND_MAX;
        if (roll < TGEN_DROP_PROB) {
            flow_signals_update(flow, SIG_RTO, 1);
        } else if (roll < TGEN_DROP_PROB + TGEN_NACK_PROB) {
            flow_signals_update(flow, SIG_NACK, 1);
        } else {
            flow_signals_update(flow, SIG_RTT, TGEN_RTT);
            flow_signals_update(flow, SIG_ACK, 1);
            if (roll < TGEN_ECN_CONG_PROB) {
                flow_signals_update(flow, SIG_ECN, 1);
            }
        }
        usleep(TGEN_THREAD_SLEEP_TIME_US);
        fprintf(stdout,
                "[flow=%p] traffic generator stats: "
                "pkts_per_ms=%d, to_send=%d, cwnd=%d\n",
                flow, pkts_per_ms, to_send, cwnd);
    }

    flow_error_status_set(flow, SUCCESS);
flow_thread_join:
    fprintf(stdout,
            "[flow=%p] traffic generation terminated with "
            "status=%d\n",
            flow, flow_error_status_get(flow));
    return NULL;
}