#!/usr/bin/env python3

"""
PCM CMake Build Script
Usage: python3 build.py [options]
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def run_command(cmd, cwd=None, check=True):
    """Run a command and handle errors."""
    print(f"Running: {' '.join(cmd)}")
    try:
        result = subprocess.run(cmd, cwd=cwd, check=check, capture_output=False)
        return result.returncode == 0
    except subprocess.CalledProcessError as e:
        print(f"Command failed with exit code {e.returncode}")
        return False
    except FileNotFoundError:
        print(f"Command not found: {cmd[0]}")
        return False


def get_cpu_count():
    """Get the number of CPU cores for parallel building."""
    try:
        import multiprocessing
        return multiprocessing.cpu_count()
    except:
        return 4


def main():
    parser = argparse.ArgumentParser(
        description="PCM CMake Build Script",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    # Build options
    parser.add_argument('--debug', action='store_true',
                       help='Build in Debug mode (default: Release)')
    parser.add_argument('--relwithdebinfo', action='store_true',
                       help='Build in RelWithDebInfo mode (optimized with debug symbols)')
    parser.add_argument('--htsim-dir', metavar='DIR',
                       help='Set HTSIM build directory')
    parser.add_argument('--hac-ir', action='store_true',
                       help='Enable HAC LLVM IR generation')
    parser.add_argument('--profiling', action='store_true',
                       help='Enable profiling support')
    parser.add_argument('--profiling-backend', choices=['perf', 'chrono', 'rdtsc'],
                       default='rdtsc',
                       help='Select profiling backend (perf/chrono/rdtsc). Default: rdtsc')
    parser.add_argument('--clean', action='store_true',
                       help='Clean build directory first')
    parser.add_argument('--install', action='store_true',
                       help='Install after building')
    parser.add_argument('--build-dir', metavar='DIR',
                       help='Build directory (default: ./build)')
    parser.add_argument('--jobs', '-j', type=int, metavar='N',
                       help=f'Number of parallel jobs (default: {get_cpu_count()})')
    
    args = parser.parse_args()
    
    # Set defaults
    if args.debug and args.relwithdebinfo:
        print("Error: Cannot specify both --debug and --relwithdebinfo")
        return 1
    
    if args.debug:
        build_type = "Debug"
    elif args.relwithdebinfo:
        build_type = "RelWithDebInfo"
    else:
        build_type = "Release"

    build_hac_ir = "ON" if args.hac_ir else "OFF"
    enable_profiling = "ON" if args.profiling else "OFF"
    jobs = args.jobs if args.jobs else get_cpu_count()
    
    # Paths
    project_root = Path(__file__).parent.absolute()
    build_dir = Path(args.build_dir) if args.build_dir else project_root / "build"
    
    print("=== PCM CMake Build ===")
    print(f"Build type: {build_type}")
    print(f"HAC IR generation: {build_hac_ir}")
    print(f"Profiling: {enable_profiling}")
    if args.profiling:
        print(f"Profiling backend: {args.profiling_backend}")
    print(f"Clean build: {args.clean}")
    print(f"Jobs: {jobs}")
    print(f"Project root: {project_root}")
    print(f"Build directory: {build_dir}")
    print()
    
    # Clean build directory if requested
    if args.clean and build_dir.exists():
        print("Cleaning build directory...")
        shutil.rmtree(build_dir)
    
    # Create build directory
    build_dir.mkdir(parents=True, exist_ok=True)
    
    # Configure CMake
    cmake_args = [
        "cmake",
        f"-DCMAKE_BUILD_TYPE={build_type}",
        f"-DBUILD_HAC_IR={build_hac_ir}",
        f"-DENABLE_VM_PROFILING={enable_profiling}",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    ]

    if args.profiling:
        cmake_args.append(f"-DVM_PROF_BACKEND={args.profiling_backend}")

    cmake_args.append(str(project_root))
    
    if args.htsim_dir:
        cmake_args.insert(-1, f"-DHTSIM_ROOT_DIR={args.htsim_dir}")
    
    print("Configuring...")
    if not run_command(cmake_args, cwd=build_dir):
        print("Configuration failed!")
        return 1
    
    # Build
    print("Building...")
    build_cmd = ["cmake", "--build", ".", "--parallel", str(jobs)]
    if not run_command(build_cmd, cwd=build_dir):
        print("Build failed!")
        return 1
    
    # Install if requested
    if args.install:
        print("Installing...")
        install_cmd = ["cmake", "--build", ".", "--target", "install"]
        if not run_command(install_cmd, cwd=build_dir):
            print("Installation failed!")
            return 1
    
    print()
    print("=== Build Complete ===")
    print(f"Build directory: {build_dir}")
    print(f"Binaries: {build_dir / 'bin'}")
    print(f"Libraries: {build_dir / 'lib'}")
    
    # Show what to do next
    print()
    print("To test the build:")
    print(f"  cd {build_dir}")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
