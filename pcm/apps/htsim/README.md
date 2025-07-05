# HTSIM App

## Running HTSIM Smartt

```
./datacenter/htsim_uec_entry_modern -o uec_entry -k 1 -algorithm smartt -nodes 1024 -q 4452000 -strat ecmp_host_random2_ecn -number_entropies 256 -kmin 20 -kmax 80 -target_rtt_percentage_over_base 50 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 800000 -mtu 4096 -seed 919 -queue_type composite  -hop_latency 700 -reuse_entropy 1 -tm ../../HTSIM/sim/datacenter/connection_matrices/incast_8_1MB.cm -x_gain 2 -y_gain 2.5 -w_gain 2 -z_gain 0.8  -collect_data 1 &> smartt.log
```

`python3 ../../pcm-sdk/pcm/apps/htsim/htsim_uec_cnwd_parser.py ./smartt.log`