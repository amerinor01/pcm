# Programmable Congestion Management (PCM) SDK

![](pcm_arch.png)

## Project structure
- `./include` - contains API definitions of PCM and abstract NIC that supports PCM
- `./src` - contains implementation of the PCM and NIC APIs
- `./algorithms` - contains examples of PCM-based congestion control algorithms: NewReno, DCTCP, DCQCN, Swift, SMaRTT
- `./apps` - contains applications
    - `traffic_gen` - runs N threads (flows), each randomly generating ACKs/NACKs/RTOs/ECNs and served by a chosen CC algo
    - `htsim` - the HTSIM traffic simulation with PCM-based congestion control
- `./analysis` - contains scripts for performance analysis

## Running synthetic application

1. `make` - compiles whole project into `lib` (app binary) and `bin` folders
    - `CC=gcc make` - change compiler to GCC (default is Clang)
2. `export LD_LIBRARY_PATH=$(pwd)/lib/:$LD_LIBRARY_PATH`
3. `export PATH=$(pwd)/bin/:$PATH`
4. `traffic_gen_app 1 10000000 dctcp &> dctcp.log` runs single flow for 10 seconds (10000000 us) and outputs log into the `dctcp.log` file
5. `python3 ./apps/traffic_gen/cwnd_parser.py dctcp.log cwnd.png` - parse log from the previous step and produces congestion window evolution plot on the screen and into `cwnd.png`:
![](dctcp_cwnd.png)

### Algorithm profiling (Linux only)
1. `ENABLE_PROFILING=1 make` - rebuild SDK with the profiling enabled
2. `sudo sysctl -w kernel.perf_event_paranoid=0` - enable collection of perf data without root permissions
3. `analysis/perftest_run.sh $(pwd)/lib/ $(pwd)/perf.out` - collect performance counters for all algorithms:
    - first argument is path to the `.so` libraries of algorithms
    - each algorithm is logged in `$(pwd)/perf.out` directory which is created if doesn't exist
4. `python3 ./analysis/plot_cyc_and_inst.py $(pwd)/perf.out` - generate violin plots for cycles and instructions inside the `$(pwd)/perf.out`

## Running htsim

For now, the supported htsim version is `spcl/HTSIM` on `lgs_stable` branch.
1. Edit root `Makefile` to supply the path to the htsim directory that contains `libhtsim.a` and all related htsim sources
2. `BUILD_HTSIM_FLOW_PLUGIN=1 make` - Note: all warnings are coming from the htsim side
3. `./bin/htsim_flow_app -o uec_entry -k 1 -algorithm smartt -nodes 1024 -q 4452000 -strat ecmp_host_random2_ecn -number_entropies 256 -kmin 20 -kmax 80 -target_rtt_percentage_over_base 50 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -seed 919 -queue_type composite -reuse_entropy 1 -tm ./apps/htsim/incast_8_1MB.cm -x_gain 2 -y_gain 2.5 -w_gain 2 -z_gain 0.8  -collect_data 1 -pcm_algorithm smartt &> pcm.log`
4. `python3 apps/htsim/htsim_pcm_cwnd_parser.py pcm.log pcm_cwnd.png`:
![](smartt_htsim.png)