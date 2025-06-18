# Overview of congestion control algorithms

| Algorithm   | Control Parameters | Triggers | Inputs |
| ----------- | ----------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------- |
| **NewReno** | Congestion window                           | cumulative ACK<br>3 DupACKs<br>RTO<br>                                        | new cumulatively ACKed bytes                                                                                               |
| **CUBIC**   | Congestion window                | cumulative ACK<br>3 DupACKs<br>RTO<br>                                                          | current time<br>new cumulatively ACKed bytes<br>RTT sample|
| **DCTCP**   | Congestion window<br>Flow path | cumulative ACK<br>3 DupACKs<br>RTO<br>RTT timer expiry | new cumulatively ACKed bytes<br>total cumulatively ACKed bytes<br>total cumulatively ACKed bytes with ECN |
| **SMaRTT**  | Congestion window<br>Flow path                | new (single) pkt ACK<br>RTO<br>NACK<br>trimmed packet (optional) | current time<br>last packet size<br>last packet RTT<br>last packet ECN flag<br>average RTT                                   |
| **Swift**   | Congestion window<br>TX pacer delay           | new (many) pkt ACK<br>RTO<br>NACK | current time<br>number of ACKed packets<br>RTT sample<br>RTT components (network/endpoint delays)                              |
| **BBRv3**   | TX pacer rate<br>Congestion window<br>Flow path | ACK reception| current time<br>new cumulatively ACKed bytes<br>new cumulatively ACKed bytes with ECN<br>RTT sample<br>number of new drops<br>number of in-flight packets<br>is_app_limited |
| **DCQCN**   | TX pacer rate | CNP packet received<br>alpha-decay timer expiry<br>rate-increase timer expiry<br>TX-byte-counter exceeded | None                                 |
| **NDP/EQDS**   | Per-priority EQIF credit | Pull request | EQIF Pull-queue sizes for all priorities<br>App drain rate (or size + current time?) |

## Table summary

*Unique control knobs:*
- TX:
    - Flow congestion window
    - Flow TX pacer rate
    - Flow path
- EQDS-specific:
    - EQIF credit for a given priority

*Unique triggers:*
- Reception of specific packet:
    - cumulative ACK
    - single-pkt ACK
    - selective ACK
    - 3 dup ACKs
    - NACK
    - Trimmed packet
    - CNP
- Timeout:
    - RTO
    - Timer
- Datapath statistic:
    - N bytes injected
    - N bytes ACK'ed
    - N bytes received
- EQDS-specific:
    - New pull request

*Unique inputs:*
- Current time
- multi-packet statistics:
    - new cumulatively ACKed bytes
    - new cumulatively ACKed bytes with ECN
    - total cumulatively ACKed bytes
    - total cumulatively ACKed bytes with ECN
    - number of newly ACKed packets
    - number of newly ACKed packets with ECN
    - number of new drops
    - number of in-flight packets
    - is-app-limited (enqueue rate or backlog size?)
    - RTT average
- per-(last)-packet statistics
    - size
    - is ECN set?
    - RTT:
        - sample
        - components break down: network/endpoint delay
- EQDS-specific:
    - Pull-queue sizes of all priorities
    - Per-priority drain rate

## NewReno

**Notes:**
    - [https://github.com/google/bbr/blob/v3/net/ipv4/tcp_cong.c](https://github.com/google/bbr/blob/v3/net/ipv4/tcp_cong.c)

**Control:**
   - Congestion window

### Algorithm logic

1. **Slow-Start vs. Congestion Avoidance**
   - **Trigger:** cumulative ACK
   - **Inputs:** number of new cumulatively ACKed bytes
   - **Effect upon Slow Start** (when `cwnd < ssthresh`):
        - on each ACK call `tcp_slow_start(..)`,
        ```c
        cwnd += MSS;  // exponential growth
        ```
   -  **Effect upon Congestion Avoidance** (when `cwnd ≥ ssthresh`): 
        - on each cumulative ACK of N MSS,  
        ```c
        acked += N;
        if (acked >= cwnd/MSS) {
            cwnd += MSS;
            acked -= cwnd/MSS;
        }
        ```

2. **Fast Recovery (on loss detected with 3 DupACKs)**  
   - **Trigger:** arrival of the 3rd duplicate ACK.  
   - **Action:**
     ```c
     ssthresh = max(cwnd/2, 2*MSS);
     cwnd = ssthresh + 3*MSS; // enters Fast recovery
     ```
   - **Each additional DupACK:**  
     ```c
     cwnd += MSS;
     ``` 
   - **On the first “new” cumulative ACK beyond the lost packet:**  
     ```c
     cwnd = ssthresh; // enters congestion avoidance
     ```

3. **RTO aka Fast Recovery**  
   - **Trigger:** RTO fires (no ACK for RTO interval).  
   - **Action:**  
     ```c
     ssthresh = max(cwnd/2, 2*MSS);
     cwnd = MSS; // enters slow start
     ```

## DCTCP

**Notes:**
    - [https://github.com/google/bbr/blob/v3/net/ipv4/tcp_dctcp.c](https://github.com/google/bbr/blob/v3/net/ipv4/tcp_dctcp.c)
    - [https://web.stanford.edu/~balaji/papers/10datacenter.pdf](https://web.stanford.edu/~balaji/papers/10datacenter.pdf)

- **Control:**
    - Congestion window
    - Flow path

### Algorithm logic

1. **Slow start and congestion avoidance**  
   - **Kernel callback** `.cong_avoid()`
   - **Trigger:** cumulative ACK
   - **Inputs:** number of new cumulatively ACKed bytes
   - **Action:** identical to NewReno (`.cong_avoid = newreno_cong_avoid`.)

2. **Alpha update**
   - **Kernel callback** `.in_ack_event()`
   - **Trigger:** RTT timer expired (tracked through seqNo's in kernel)
   - **Inputs:** 
        - total number of cumulatively ACKed bytes since socket creation
        - total number of cumulatively ECNed bytes since socket creation
   - **Action:** 
        - update of fraction of ECN’ed bytes (alpha, internal state)
        ```c
        if (snd_una >= next_seq) { // RTT of packets got ACKed
            alpha = (1–g) * alpha + g * (acked_bytes_ecn/acked_bytes_total);
            reset_window_marker();
        }
        ```
     where:
     - **acked_bytes_total** - number of cumulatively ACKed bytes since last *reset_window_marker()*
     - **acked_bytes_ecn** - number of cumulatively ACKed bytes with ECN bit set since last *reset_window_marker()*
    - **Extension in recent kernels:** Flow path change

3. **Fast Recovery/Retransmit**
   - Similar to NewReno, but with DCTCP-specific `.ssthresh()` logic computation:
     ```c
     ssthresh = cwnd * (1 – alpha/2);
     ```
    - **Extension in recent kernels:** Flow path change

## CUBIC

**Notes:**
    - [https://github.com/google/bbr/blob/v3/net/ipv4/tcp_cubic.c](https://github.com/google/bbr/blob/v3/net/ipv4/tcp_cubic.c)
    - [https://www.cs.princeton.edu/courses/archive/fall16/cos561/papers/Cubic08.pdf](https://www.cs.princeton.edu/courses/archive/fall16/cos561/papers/Cubic08.pdf)

- **Control:**
    - Congestion window
    - Flow path

### Algorithm logic

1. **Slow Start** when `cwnd < ssthresh`: 
   - **Kernel callback** `.cong_avoid()`
   - **Trigger:** cumulative ACK identical to NewReno, on each ACK
   - **Inputs:**
        - number of new cumulatively ACKed packets
        - min RTT sample `RTT`
   - **Action:** identical to NewReno in slow-start phase + update of min_delay (see CA stage)

2. **Congestion Avoidance** call to `.cong_avoid` when `cwnd >= ssthresh`:
   - **Kernel callback** `.cong_avoid()`
   - **Trigger:** cumulative ACK
   - **Inputs:**
      - current time `T_now`
      - min RTT sample `RTT` (recorded in parralel ub `.pkts_acked()` callback)
   - **Optional inputs:** number of cumulatively ACKed packets (optional, needed if TCP friendliness enabled)
   - **Action:** 
     - `cwnd` follows a cubic function of time since the last congestion event:
     ```c
     // simplified logic (no TCP friendliness)
     min_delay = min(min_delay, RTT)
     t = T_now + min_delay - t_cwnd_decr;
     K = cbrt(W_max*beta/C)
     W_cubic(t) =  W_max + C*((t–K)^3)
     if (W_cubic(t) > cwnd) {
        cnt = cwnd / (W_cubic(t)-cwnd)
     } else {
        cnt *= 100
     }
     // update congestion window if threshold reached
     if (cwnd_cnt > cnt) {
          cwnd += MSS
          cwnd_cnt = 0
     } else {
          cwnd_cnt++
     }
     ```
     where:
     - **t_cwnd_decr** time when last cwnd decrease happened
     - **min_delay** minimum RTT observed
     - **W_max** is the value of `cwnd` just before that reduction. 
     - **cwnd_cnt** is internal threshold to track when it’s time to inrecment congestion window
     - **beta** is the multiplicative decrease factor (default 0.2).  
     - **C** is a scaling constant (default 0.4).

3. **Fast recovery/retransmit**
   - **Triggers:** 
      - RTO
      - loss
   - **Action:**
        - Similar to NewReno, but with CUBIC-specific `.ssthresh()` callback that also updates internal state:
        ```c
        ssthresh = cwnd * (1-beta);
        if (cwnd < W_max) {
            W_max = cwnd * (2-beta)/2;
        } else {
            W_max = cwnd
        }
        t_cwnd_decr = T_now()
        ```

## BBRv3

[https://github.com/google/bbr/blob/v3/net/ipv4/tcp_bbr.c](https://github.com/google/bbr/blob/v3/net/ipv4/tcp_bbr.c)

- **Notes:**
    - BBR code base is huge and complicated >2k LoC of C code in Linux kernel

- **Control:**  
    - TX pacer rate
    - Congestion window
    - Flow path

### Algorithm logic

Progress in recent version of BBR is fully driven by calling `.cong_control(rs)` callback upon ACK reception:

- **Trigger:** ACK reception
- **Kernel callback:** `.cong_control(rs)` callback in the kernel which calls to `bbr_main()`
- **Input:**
   - **Rate sample:** fields leveraged:
      - `rs.delivered` delivered bytes within a sample
      - `rs.delivered_ce` delivered bytes with ECN within a sample (used in BBRv3)
      - `rs.interval_us` sample interval
      - `rs.rtt` RTT measurement of the ACK sample
      - `rs.losses` number of losses within a rate sample
      - `rs.is_app_limited` there is a ‘bubble’ in the TX pipe, e.g., APP can’t saturate BW 
   - Number of packets in flight (part of Linux TCP socket struct)
   - Current time

## DCQCN

**Notes:**
- DCQCN version considered here is based on analysis of original DCQCN paper: [https://conferences.sigcomm.org/sigcomm/2015/pdf/papers/p523.pdf](https://conferences.sigcomm.org/sigcomm/2015/pdf/papers/p523.pdf)
    - the algo in the paper looks to be heavily adapted to work on the Mellanox/NVIDIA NICs with their funky PCC logic (e.g., ECN signals are not delivered with ACKs, but receiver sends them in-band using special CNP packets if ECN'ed packet was observed in last XX nanosec)
    - See `./dcqcn.c` for sketch implementation
    - **TODO:** check other DCQCN implementaions as they might differ 
    - Broadcom's DCQCN variants: ([https://docs.broadcom.com/doc/NCC-WP1XX](https://docs.broadcom.com/doc/NCC-WP1XX))

- **Control:**
    - TX pacer rate

### Algorithm logic

1. **Reaction to congestion**:
    - **Trigger:** Congestion Notification Packet (CNP) received
    - **Input:** None
    - **Action:** 
        - Rate decrease according to DCQCN law:
            ```c
            /* record old rate as target rate */
            s->rate_tgt = s->rate_cur;
            /* multiplicative decrease factor */
            s->rate_cur *= (1 - 0.5 * s->alpha);
            /* update alpha */
            s->alpha = (1 - DCQCN_GAMMA) * s->alpha + DCQCN_GAMMA;
            ```
        - Reset of alpha and rate increase timers

2. **Alpha update**
    - **Trigger:** Since last congestion, alpha update timer expired
    - **Input:** None
    - **Action:** Alpha (fraction of ECN'ed bytes, internal state) decreased similarly to DCTCP algo
        ```c
        s->alpha = (1 - DCQCN_GAMMA) * s->alpha;
        ```

3. **Rate increase**
    - **Trigger 1:** Since last congestion, rate increase timer expired
    - **Trigger 2:** Since last congestion, B bytes were injected into the network
    - **Input:** None
    - **Action:** Rate increase according to DCQCN law (Fast Recovery -> Hyper Increase -> Additive Increase)
        ```c
        // simplified logic based on paper
        if (s->rate_cur >= s->rate_max)
            return;
        uint32_t min_counter;
        if (max(s->rate_increase_timer_evts, s->byte_counter_evts) < DCQCN_FR_STEPS) {
            /* Fast Recovery */
            //s->rate_tgt = s->rate_cur;
        } else if ((min_counter = min(s->rate_increase_timer_evts, s->byte_counter_evts)) > DCQCN_FR_STEPS) {
            /* Hyper Increase (optional?) */
            s->rate_tgt += min_counter * DCQCN_RHAI;
        } else {
            /* Additive Increase */
            s->rate_tgt += DCQCN_RAI;
        }
        s->rate_cur = (s->rate_tgt + s->rate_cur) / 2;
        if (s->rate_cur > s->rate_max)
            s->rate_cur = s->rate_max;
        ```

## Swift

**Notes:**
    - [https://research.google/pubs/swift-delay-is-simple-and-effective-for-congestion-control-in-the-datacenter/](https://research.google/pubs/swift-delay-is-simple-and-effective-for-congestion-control-in-the-datacenter/)
    - See `./swift.c` for sketch implementation

- **Control:**
    - Congestion window
    - TX pacer delay (optional?)

### Algorithm logic

1. **Active control**
    - **Trigger:** New packets (even selectively?) got ACKed
    - **Input:**
        - Current time
        - Number of ACKed packets
        - RTT sample
        - RTT components (network/endpoint delays) (optional?)
    - **Action:**
        - If current delay is above target delay, and decrease is allowed, rate is decreased
        - Otherwise, rate is increased if allowed

2. **Reaction to congestion signals**
    - **Trigger 1:** RTO
    - **Trigger 2:** NACK received (e.g., received detected loss and indicated it in SACK bitmap)
    - **Input:** Current time
    - **Action:**
        - congestion window decrease according to Swift law
        - record time of last decrease

## SMaRTT

**Notes:**
    - See `./smartt.c` for sketch implementation

- **Control:**
    - Congestion window
    - Flow path

### Algorithm logic

1. **Active control**
    - **Trigger:** New (even selective) ACK got received
    - **Input:**
        - Current time
        - Last packet size
        - Last packet RTT
        - Last packet is ECN?
        - Average RTT
    - **Effect**
        - SmaRTT logic to adjust window: QuckAdapt/FastIncrease or core cases (MD/FI/PI).
        - Possible path change

2. **Reaction to congestion signals**
    - **Trigger 1:** RTO
    - **Trigger 2:** NACK received (e.g., received detected loss and indicated it in SACK bitmap)
    - **Trigger 3: (optional)** Trimmed packet got received
    - **Input:**
        - Current time
        - Last packet size
    - **Action:**
        - Decrease congestion window by size of last packet 
        - Trigger QuickAdapt

# Receiver-based control

## EQDS/NDP:

**Note:**
- In EQDS/NDP a 'control' is a process of granting credits at the receiver (basically receiver admits senders by allocating them bandwidth)
- **What to do with sender?**

**Control:**
- Receiver maintains a single FIFO-like (?) Pull-queue for all senders 
- Every received packet or trimmed header is logged in Pull-queue (inserted into tail)
- Receiver polls head of Pull-queue, and grants credit to the flow at the head of queue and dequeues it
- All granted credit covers available receive bandwidth

**Open questions:**
- What do we want to control?
- Can we have multiple pull queues (CCC per priority??) in UE and arbitrate their credit (allocate bandwidth) in SW?