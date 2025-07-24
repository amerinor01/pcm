#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "traffic_gen.h"
#include "pcm_network.h"
#include "pcm.h"

// Flow thread: emulate constant bandwidth, drops, NACKs, RTOs
void *app_flow_traffic_gen_fn(void *arg) {
    pcm_flow_t flow = arg;

    unsigned int rnd = (unsigned int)time(NULL);
    const int pkts_per_ms = (TGEN_BANDWIDTH_BPS / 8 / 1000) / TGEN_PACKET_SIZE;

    while (!flow_is_ready(flow)) {
        ; // datapath might still need to finish setup (arm triggers)
    }

    fprintf(stdout, "[app_flow=%p] started\n", flow);

    while (flow_is_ready(flow)) {
        int cwnd = flow_cwnd_get(flow);
        if (cwnd == 0) {
            usleep(TGEN_THREAD_SLEEP_TIME_US);
            continue;
        }
        int to_send = cwnd < pkts_per_ms ? cwnd : pkts_per_ms;
        flow_signals_update(flow, PCM_SIG_DATA_TX, to_send * TGEN_MSS);
        double roll = (double)rand_r(&rnd) / RAND_MAX;
        if (roll < TGEN_DROP_PROB) {
            flow_signals_update(flow, PCM_SIG_RTO, 1);
        } else if (roll < TGEN_DROP_PROB + TGEN_NACK_PROB) {
            flow_signals_update(flow, PCM_SIG_NACK, 1);
        } else {
            flow_signals_update(flow, PCM_SIG_RTT, TGEN_RTT);
            flow_signals_update(flow, PCM_SIG_ACK, 1);
            if (roll < TGEN_ECN_CONG_PROB) {
                flow_signals_update(flow, PCM_SIG_ECN, 1);
            }
        }
        usleep(TGEN_THREAD_SLEEP_TIME_US);
        fprintf(stdout,
                "[app_flow=%p] stats: pkts_per_ms=%d, to_send=%d, cwnd=%d\n",
                flow, pkts_per_ms, to_send, cwnd);
    }

    fprintf(stdout, "[app_flow=%p] terminated\n", flow);
    return NULL;
}