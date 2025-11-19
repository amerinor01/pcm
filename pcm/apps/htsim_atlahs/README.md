# Application htsim_atlahs Build & Run Guide

## 1. Clone Repository and Submodules

``` bash
cd pcm-sdk

# make sure to pull submodules recursively
git submodule update --init --recursive
```

## 2. Build `uet-htsim`

``` bash
# make sure to apply uet-htsim-patch to uet-htsim first
cd uet-htsim/htsim/sim

cmake -S . -B build
cmake --build build --parallel
```

## 3. Build `HTSIM_spcl`

``` bash
# make sure to apply HTSIM_spcl-patch to HTSIM_spcl first
cd HTSIM_spcl/htsim/sim

cmake -S . -B build
cmake --build build --parallel
```

## 4. Build PCM

``` bash
cd pcm

cmake -S . -B build -DBUILD_HTSIM_PLUGIN=ON
cmake --build build --parallel
```

## 5. Run Applications

### CM Mode

``` bash
cd pcm

python ./apps/htsim_atlahs/batch_simulations_tm.py   --conf ./apps/htsim_atlahs/all_algos_incast.json   --out ./apps/htsim_atlahs/log_tm   --plot
```

### Goal Mode

``` bash
cd pcm

python ./apps/htsim_atlahs/batch_simulations_goal.py   --conf ./apps/htsim_atlahs/pcm_goal_allreduce.json   --out ./apps/htsim_atlahs/log_goal/all_reduce   --plot
```

