#!/usr/bin/env python3

"""
Batch experiment runner for PCM SDK and libccp testbench.
Replaces run_experiments.sh with a more robust Python implementation.
"""

import argparse
import subprocess
import sys
import os
from pathlib import Path
import time

# --- Configuration ---
ITERS = 10000
SEED = 42
NO_LOSS = 0

ALGOS = [
    "newreno", "dctcp", "swift", "smartt", "nscc", "uec_dctcp",
    "strack", "strack_light", "ops_lb", "reps", "cubic",
    "uec_dctcp_v2", "newreno_v2", "dctcp_v2", "cubic_v2", "swift_v2",
    "nscc_v2", "smartt_v2", "ops_lb_v2", "reps_v2", "strack_v2", "strack_light_v2"
]

MODES = ["sync", "async"]

# Build Configurations
CONFIGS = [
    {
        "name": "vm-shell-overhead-rdtsc",
        "pcm_args": ["--profiling", "--profiling-backend=rdtsc"],
        "tb_args": []
    },
    {
        "name": "vm-shell-overhead-perf",
        "pcm_args": ["--profiling", "--profiling-backend=perf"],
        "tb_args": []
    },
    {
        "name": "tb-submit-rdtsc",
        "pcm_args": [],
        "tb_args": ["PROFILE_SUBMIT=y"]
    },
    {
        "name": "tb-submit-perf",
        "pcm_args": [],
        "tb_args": ["TIMING_BACKEND=PERF", "PROFILE_SUBMIT=y"]
    },
    {
        "name": "tb-full-cycle-rdtsc",
        "pcm_args": [],
        "tb_args": ["PROFILE_FULL_CYCLE=y"]
    },
    {
        "name": "tb-full-cycle-perf",
        "pcm_args": [],
        "tb_args": ["TIMING_BACKEND=PERF", "PROFILE_FULL_CYCLE=y"]
    },
    {
        "name": "tb-async-invoke-rdtsc",
        "pcm_args": [],
        "tb_args": ["PROFILE_ASYNC_INVOKE=y"]
    },
    {
        "name": "tb-async-invoke-perf",
        "pcm_args": [],
        "tb_args": ["TIMING_BACKEND=PERF", "PROFILE_ASYNC_INVOKE=y"]
    },
]

#ATOMIC_OPTS = [
#    {"suffix": "", "args": []},
#    {"suffix": "-aligned", "args": ["--aligned-atomic-storage"]}
#]

ATOMIC_OPTS = [
    {"suffix": "", "args": []}
]

def run_command(cmd, cwd, log_file_path=None, check=True):
    """
    Run a command in a specific directory, optionally logging output to a file.
    """
    cmd_str = " ".join(cmd)
    # print(f"    [Exec] {cmd_str}")
    
    stdout_dest = subprocess.PIPE
    stderr_dest = subprocess.STDOUT
    
    if log_file_path:
        # Open file for writing (this will truncate existing file)
        with open(log_file_path, "w") as f:
            try:
                subprocess.run(cmd, cwd=cwd, check=check, stdout=f, stderr=subprocess.STDOUT)
                return True
            except subprocess.CalledProcessError as e:
                print(f"    [Error] Command failed: {cmd_str}")
                print(f"    [Log] Check {log_file_path} for details.")
                if check:
                    sys.exit(1)
                return False
    else:
        try:
            subprocess.run(cmd, cwd=cwd, check=check)
            return True
        except subprocess.CalledProcessError as e:
            print(f"    [Error] Command failed: {cmd_str}")
            if check:
                sys.exit(1)
            return False

def main():
    parser = argparse.ArgumentParser(description="PCM Batch Experiment Runner")
    parser.add_argument("pcm_dir", type=Path, help="Path to PCM SDK directory")
    parser.add_argument("ccp_dir", type=Path, help="Path to libccp directory")
    parser.add_argument("log_root", type=Path, help="Root directory for logs")
    
    args = parser.parse_args()
    
    pcm_dir = args.pcm_dir.resolve()
    ccp_dir = args.ccp_dir.resolve()
    log_root = args.log_root.resolve()
    
    if not pcm_dir.exists():
        print(f"Error: PCM directory not found: {pcm_dir}")
        sys.exit(1)
    if not ccp_dir.exists():
        print(f"Error: CCP directory not found: {ccp_dir}")
        sys.exit(1)

    print("==================================================")
    print("PCM Batch Experiment Runner (Python)")
    print(f"PCM Directory: {pcm_dir}")
    print(f"CCP Directory: {ccp_dir}")
    print(f"Log Root:      {log_root}")
    print(f"Iterations:    {ITERS}")
    print(f"Seed:          {SEED}")
    print("==================================================")

    log_root.mkdir(parents=True, exist_ok=True)

    # Calculate HTSIM directory relative to PCM directory
    # Assuming structure: .../uet-htsim/htsim/sim/
    # And pcm_dir is: .../pcm-sdk/pcm/
    # So we go up two levels from pcm_dir to get to the common root
    # This matches the shell script logic: $(dirname "$PCM_DIR")/uet-htsim/htsim/sim/
    htsim_dir = pcm_dir.parent / "uet-htsim" / "htsim" / "sim"

    for atomic_opt in ATOMIC_OPTS:
        for config in CONFIGS:
            # Skip perf-based configs when using aligned atomic storage
            if "--aligned-atomic-storage" in atomic_opt["args"]:
                is_perf_config = False
                if any("perf" in arg for arg in config["pcm_args"]):
                    is_perf_config = True
                if any("TIMING_BACKEND=PERF" in arg for arg in config["tb_args"]):
                    is_perf_config = True
                
                if is_perf_config:
                    continue

            exp_name = config["name"] + atomic_opt["suffix"]
            pcm_args = config["pcm_args"] + atomic_opt["args"]
            tb_args = config["tb_args"]
            
            print(f"\n>>> Starting Experiment Set: {exp_name}")
            print(f"    PCM Args:   {' '.join(pcm_args) if pcm_args else '[Default]'}")
            print(f"    TB Args:    {' '.join(tb_args) if tb_args else '[Default]'}")
            
            exp_log_dir = log_root / exp_name
            exp_log_dir.mkdir(parents=True, exist_ok=True)
            
            # 1. Rebuild PCM Framework
            print("    [Build] Rebuilding PCM...")
            build_cmd = ["./build.py", "--clean", f"--htsim-dir={htsim_dir}", "--relwithdebinfo"] + pcm_args
            run_command(build_cmd, cwd=pcm_dir, log_file_path=exp_log_dir / "build_pcm.log")
            
            # 2. Rebuild Testbench
            print("    [Build] Rebuilding Testbench...")
            # Clean first
            run_command(["make", "clean"], cwd=ccp_dir, check=False) # Don't fail if clean fails
            
            # Build
            make_cmd = ["make", "testbench_with_portus"] + tb_args
            run_command(make_cmd, cwd=ccp_dir, log_file_path=exp_log_dir / "build_tb.log")
            
            # 3. Run Experiments
            print("    [Run] Starting iterations...")
            
            for algo in ALGOS:
                for mode in MODES:
                    log_filename = f"{exp_name}_{algo}_{mode}_{SEED}_{ITERS}.log"
                    log_file = exp_log_dir / log_filename
                    
                    print(f"        Running {algo} ({mode})... ", end="", flush=True)
                    
                    # ./testbench_with_portus [iters] [seed] [no_loss] [mode] [algo] [pcm_mode]
                    run_cmd = [
                        "./testbench_with_portus",
                        str(ITERS),
                        str(SEED),
                        str(NO_LOSS),
                        "pcm",
                        algo,
                        mode
                    ]
                    
                    success = run_command(run_cmd, cwd=ccp_dir, log_file_path=log_file, check=False)
                    
                    if success:
                        print("Done.")
                    else:
                        print("Failed! (Check log)")

    print("\n==================================================")
    print("All experiments completed.")
    print(f"Logs stored in: {log_root}")
    print("==================================================")

if __name__ == "__main__":
    main()
