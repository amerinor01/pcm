# Programmable Congestion Management SDK

This repository contains the source code for the PCM SDK.

PCM is a research and development SDK for implementing and evaluating programmable congestion control (CC) and load balancing (LB) algorithms. It provides CPU-based implementations of PCM algorithm interfaces and a runtime shell.

To compare supported algorithms under traffic patterns such as incast and core congestion, the project also integrates with UET htsim and ATLAHS/HTSIM toolchains, enabling both baseline network experiments and trace-driven studies.

## SDK structure

*SDK core:*
- `build.py`: primary build entry point.
- `hac/`: code generation and helper tooling for algorithm integration.
- `algorithms/`: CC/LB algorithms.
- `include/`: PCM runtime.

*Applications:*
- `apps/htsim/`: UET htsim-based simulation app, scenarios, configs, and plotting scripts.
- `apps/htsim_atlahs/`: ATLAHS/GOAL-trace simulation app and related batch workflows.
- `third-party/`: external dependencies.


# Testing PCM with applications

## Dependencies

- C++23 compiler
- cmake >= 3.16
- numpy
- matplotlib
- pyyaml

## Ultra Ethernet htsim application

### 1. Build UET htsim:

```bash
git submodule update --init --recursive
# The htsim patch exposes parts of the UE NIC/endpoint interface, 
# allowing CC/LB logic inside the UE stack to be redirected into PCM.
cp -r ./apps/htsim/uet-htsim-patch/* ./third-party/uet-htsim/htsim/sim/
cd ./third-party/uet-htsim/htsim/sim/
cmake -S . -B build
cmake --build build --parallel
```

### 2. Build `xxHash`

Load balancing algorithms rely on the xxHash library to provide fast PRNG:
```bash
cd ./third-party/xxHash && make
```

### 3. Build PCM runtime and htsim application

The PCM project can be built using `build.py` script which acts as a wrapper for CMake:

```bash
./build.py --htsim-dir=$(pwd)/third-party/uet-htsim/htsim/sim/ --relwithdebinfo
```

### 4. Test PCM congestion control algorithms with an incast traffic matrix

The following command can be used to run batch incast experiments for all supported CC algorithms:

```bash
export LD_LIBRARY_PATH=$(pwd)/build/lib/:$LD_LIBRARY_PATH
python3 ./apps/htsim/batch_simulations.py --conf=$(pwd)/apps/htsim/all_algos_incast.json --out=$(pwd)/results --plot
```

Upon successful execution, the `./results/` folder will contain congestion window evolution PDF plots for all algorithms listed in the `all_algos_incast.json` batch.

Other experiments (core congestion, LB algorithms) can be run using JSON batch configs in `./apps/htsim/`.

## ATLAHS+PCM: Testing PCM with GOAL application traces

PCM is integrated with the ATLAHS simulation toolchain, which extends UET htsim to support simulation of application GOAL traces.

See [https://github.com/spcl/HTSIM](https://github.com/spcl/HTSIM) for more information.

### 1. Build `HTSIM_spcl`

As with basic htsim, build spcl/HTSIM.

```bash
# make sure to pull submodules recursively
git submodule update --init --recursive
# apply HTSIM_spcl-patch to HTSIM_spcl first
cp -r ./apps/htsim_atlahs/HTSIM_spcl-patch/* ./third-party/HTSIM_spcl/htsim/sim/
# build htsim-atlahs
cd ./third-party/HTSIM_spcl/htsim/sim/
cmake -S . -B build
cmake --build build --parallel
```

### 2. Build `xxHash`

The procedure is similar to the UET htsim app.

### 3. Build ATLAHS+PCM application

```bash
python3 build.py --relwithdebinfo --build-htsim-atlahs
```

### 4. Test ATLAHS+PCM with incast trace

```bash
python3 ./apps/htsim_atlahs/batch_simulations_goal.py --conf ./apps/htsim_atlahs/pcm_goal_incast_16_nodes.json --out ./apps/htsim_atlahs/log_goal/16_nodes --plot
```

## Useful `build.py` options

### Help

```bash
python3 ./build.py --help
```

### Cleanup build

```bash
python3 ./build.py --clean
```

### Debug build with -O0

```bash
python3 build.py --debug
```

### Profiling

```bash
python3 build.py --profiling --profiling-backend <perf,chrono,rdtsc>
```

**Notes:**
- The `perf` option is supported only on Linux. Run `sudo sysctl -w kernel.perf_event_paranoid=0` to enable collection of perf data without root permissions.
- `rdtsc` is supported only on x86.