#ifndef _FABRIC_PARAMS_
#define _FABRIC_PARAMS_

#define FABRIC_HOP_COUNT 5         // E.g., links in fat tree
#define FABRIC_LINK_RATE_GBPS (100 / 8) // GByte/s
#define FABRIC_BASE_RTT (10)       // microseconds
#define FABRIC_LINK_MTU 4096       // Bytes
#define FABRIC_BDP                                                             \
    ((FABRIC_LINK_RATE_GBPS * FABRIC_BASE_RTT) * 125) // Bytes, 125 == 1000 / 8
#define FABRIC_MIN_CWND (FABRIC_LINK_MTU)        // Bytes
#define FABRIC_MAX_CWND (FABRIC_BDP)             // Bytes

#endif /* _FABRIC_PARAMS_ */