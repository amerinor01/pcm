#ifndef _FABRIC_PARAMS_
#define _FABRIC_PARAMS_


#define FABRIC_LINK_RATE_GBPS (400 / 8) // GByte/s
#define FABRIC_HOP_COUNT 6         // E.g., links in fat tree
#define FABRIC_LINK_MSS 2048 // Bytes
#define FABRIC_BDP 252300 // Bytes
#define FABRIC_MIN_CWND (FABRIC_LINK_MSS)        // Bytes
#define FABRIC_MAX_CWND (FABRIC_BDP)             // Bytes
#define FABRIC_TRTT 7637580 // Picosec
#define FABRIC_BRTT 5058000 // Picosec

#endif /* _FABRIC_PARAMS_ */