#!/usr/bin/env python3

"""
[test]Batch experiment runner for PCM SDK and libccp testbench.
Replaces run_experiments.sh with a more robust Python implementation.
"""

import argparse
import subprocess
import sys
import os
from pathlib import Path
import time

# --- Configuration ---
ITERS = 100000
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
]

# Build Configurations
CONFIGS = [
    {
        "name": "tb-submit-rdtsc",
        "pcm_args": [],
        "tb_args": ["PROFILE_FETCH=y"]
    },
    {
        "name": "tb-submit-perf",
        "pcm_args": [],
        "tb_args": ["TIMING_BACKEND=PERF", "PROFILE_FETCH=y"]
    }
]

MAPPINGS = [
    {"suffix": "ht", "handler_core": "49", "main_core": "13"},
    {"suffix": "mc", "handler_core": "47", "main_core": "13"}
]

COMPILERS = [
    {"name": "gcc", "cc": "gcc", "cxx": "g++"},
    {"name": "clang", "cc": "clang", "cxx": "clang++"}
]

def run_command(cmd, cwd, log_file_path=None, check=True, env=None):
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
                subprocess.run(cmd, cwd=cwd, check=check, stdout=f, stderr=subprocess.STDOUT, env=env)
                return True
            except subprocess.CalledProcessError as e:
                print(f"    [Error] Command failed: {cmd_str}")
                print(f"    [Log] Check {log_file_path} for details.")
                if check:
                    sys.exit(1)
                return False
    else:
        try:
            subprocess.run(cmd, cwd=cwd, check=check, env=env)
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
    print(f"Test")
    print("==================================================")

    log_root.mkdir(parents=True, exist_ok=True)

    # Calculate HTSIM directory relative to PCM directory
    # Assuming structure: .../uet-htsim/htsim/sim/
    # And pcm_dir is: .../pcm-sdk/pcm/
    # So we go up two levels from pcm_dir to get to the common root
    # This matches the shell script logic: $(dirname "$PCM_DIR")/uet-htsim/htsim/sim/
    htsim_dir = pcm_dir.parent / "uet-htsim" / "htsim" / "sim"

    for compiler in COMPILERS:
        env = os.environ.copy()
        env["CC"] = compiler["cc"]
        env["CXX"] = compiler["cxx"]
        
        print(f"\n==================================================")
        print(f"Compiler Set: {compiler['name']} (CC={compiler['cc']}, CXX={compiler['cxx']})")
        print(f"==================================================")

        for config in CONFIGS:
            exp_name = f"{compiler['name']}-{config['name']}"
            pcm_args = config["pcm_args"]
            tb_args = config["tb_args"]
            
            print(f"\n>>> Starting Experiment Set: {exp_name}")
            print(f"    PCM Args:   {' '.join(pcm_args) if pcm_args else '[Default]'}")
            print(f"    TB Args:    {' '.join(tb_args) if tb_args else '[Default]'}")
            
            exp_log_dir = log_root / exp_name
            exp_log_dir.mkdir(parents=True, exist_ok=True)
            
            # 1. Rebuild PCM Framework
            print("    [Build] Rebuilding PCM...")
            build_cmd = ["./build.py", "--clean", f"--htsim-dir={htsim_dir}", "--relwithdebinfo"] + pcm_args
            run_command(build_cmd, cwd=pcm_dir, log_file_path=exp_log_dir / "build_pcm.log", env=env)
            
            # 2. Rebuild Testbench
            print("    [Build] Rebuilding Testbench...")
            # Clean first
            run_command(["make", "clean"], cwd=ccp_dir, check=False, env=env) # Don't fail if clean fails
            
            # Build
            make_cmd = ["make", "testbench_with_portus"] + tb_args
            run_command(make_cmd, cwd=ccp_dir, log_file_path=exp_log_dir / "build_tb.log", env=env)
            
            # 3. Run Experiments
            print("    [Run] Starting iterations...")
            
            for algo in ALGOS:
                for mode in MODES:
                    for mapping in MAPPINGS:
                        if mode == "sync" and mapping["suffix"] != "ht":
                            continue

                        log_filename = f"{exp_name}_{algo}_{mode}_{mapping['suffix']}_{SEED}_{ITERS}.log"
                        log_file = exp_log_dir / log_filename
                        
                        print(f"        Running {algo} ({mode}) [{mapping['suffix']}]... ", end="", flush=True)
                        
                        # ./testbench_with_portus [iters] [nflows] [seed] [no_loss] [mode] [algo] [pcm_mode] [handler_core] [main_core]
                        run_cmd = [
                            "./testbench_with_portus",
                            str(ITERS),
                            str(1),
                            str(SEED),
                            str(NO_LOSS),
                            "pcm",
                            algo,
                            mode,
                            mapping["handler_core"],
                            mapping["main_core"]
                        ]
                        print(f"{run_cmd}", flush=True)
                        success = run_command(run_cmd, cwd=ccp_dir, log_file_path=log_file, check=False, env=env)
                        
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
