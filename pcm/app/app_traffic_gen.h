#ifndef _APP_TRAFFIC_GEN_H_
#define _APP_TRAFFIC_GEN_H_

#define TGEN_BANDWIDTH_BPS 100000000UL // 100 Mbps
#define TGEN_DROP_PROB 0.01            // 1% packet drop probability
#define TGEN_NACK_PROB 0.02            // 2% NACK probability (duplicate ACK)
#define TGEN_ECN_CONG_PROB 0.1
#define TGEN_PACKET_SIZE 1500 // bytes per packet (MSS)
#define TGEN_THREAD_SLEEP_TIME_US 1000
#define TGEN_RTT 10
#define TGEN_MSS 4096

void *app_flow_traffic_gen_fn(void *arg);

#endif /* _APP_TRAFFIC_GEN_H_ */