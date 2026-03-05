#!/usr/bin/env python3

"""
HTSIM Batch Simulation Runner
Runs multiple simulations with different baseline and PCM algorithms.

Usage: python3 batch_simulations.py --conf=all_algos_core.json --out=results/
"""

import argparse
import json
import subprocess
import sys
from datetime import datetime
from pathlib import Path


def run_command(cmd, output_dir, test_name, create_plot=False):
    """Run command and save output."""
    test_dir = output_dir / test_name
    test_dir.mkdir(parents=True, exist_ok=True)
    
    stdout_file = test_dir / "stdout.log"
    stderr_file = test_dir / "stderr.log"
    cmd_file = test_dir / "command.txt"
    
    # Save command
    with open(cmd_file, 'w') as f:
        f.write(' '.join(cmd) + '\n')
    
    print(f"Running: {test_name}")
    print(f"  Command: {' '.join(cmd)}")
    
    try:
        with open(stdout_file, 'w') as stdout_f, open(stderr_file, 'w') as stderr_f:
            result = subprocess.run(cmd, stdout=stdout_f, stderr=stderr_f, 
                                  timeout=3000, check=False)
        
        success = result.returncode == 0
        status = "SUCCESS" if success else f"FAILED (exit {result.returncode})"
        print(f"  {status}")
        
        # Generate plot if requested and command succeeded
        if create_plot and success and stdout_file.exists():
            generate_plot(stdout_file, test_dir, test_name)
        
        return success
        
    except subprocess.TimeoutExpired:
        print("  TIMEOUT")
        return False
    except Exception as e:
        print(f"  ERROR: {e}")
        return False


def generate_plot(log_file, output_dir, test_name):
    """Generate CWND plot using the plotting script."""
    try:
        plot_script = Path(__file__).parent / "plot_cwnd.py"
        if not plot_script.exists():
            print(f"    Plot script not found: {plot_script}")
            return
        
        plot_file = output_dir / f"{test_name}_cwnd.pdf"
        
        cmd = [
            sys.executable,  # Use the same Python interpreter
            str(plot_script),
            "--input", str(log_file),
            "--output", str(plot_file),
            "--title", f"CWND Evolution - {test_name}"
        ]
        
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        
        if result.returncode == 0:
            print(f"  CWND plot saved to: {plot_file}")
            return
        else:
            print(f"    Plot generation failed")
            return
            
    except Exception as e:
        print(f"    Plot generation error: {e}")
        return


def generate_performance_plot(profile_subsyst, output_file_name, output_dir):
    """Generate cycle and instruction performance plots using plot_cyc_and_inst.py."""
    try:
        plot_script = Path(__file__).parent.parent.parent / "analysis" / "plot_cyc_and_inst.py"
        if not plot_script.exists():
            print(f"    Performance plot script not found: {plot_script}")
            return
        
        plot_file = output_dir / output_file_name
        
        cmd = [
            sys.executable,  # Use the same Python interpreter
            str(plot_script),
            str(output_dir),  # Directory containing .log files
            str(plot_file),   # Output plot file
            profile_subsyst  # Profile pattern to search for
        ]
        
        print(f"Generating performance plots...")
        print(f"  Command: {' '.join(cmd)}")
        
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        
        if result.returncode == 0:
            print(f"  Performance plots saved to: {plot_file}")
            return
        else:
            print(f"    Performance plot generation failed:")
            print(f"    stdout: {result.stdout}")
            print(f"    stderr: {result.stderr}")
            return
            
    except Exception as e:
        print(f"    Performance plot generation error: {e}")
        return

def main():
    parser = argparse.ArgumentParser(description="HTSIM Batch Simulation Runner")
    parser.add_argument('--conf', required=True, help='JSON config file')
    parser.add_argument('--out', required=True, help='Output directory')
    parser.add_argument('--plot', action='store_true', 
                       help='Generate CWND plots for successful runs')
    parser.add_argument('--profile', action='store_true',
                       help='Generate cycle and instruction performance plots')
    
    args = parser.parse_args()
    
    # Load config
    with open(args.conf, 'r') as f:
        config = json.load(f)
    
    output_dir = Path(args.out)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    binary = config.get('binary', './build/bin/htsim_flow_app_atlahs')
    input_files = config['input_files']
    htsim_params = config.get('htsim_params', '').split()
    
    # Baseline algorithm configurations
    run_without_pcm = config.get('run_without_pcm', False)
    baseline_config = config.get('baseline_config', '').split()

    native_nscc_config_file = config.get('native_nscc_config_file', [])
    pcm_cc_config_file = config.get('pcm_cc_config_file', [])
    
    # PCM algorithm configurations  
    pcm_delay_config = config.get('pcm_config', '').split()
    
    results = []
    
    print(f"=== HTSIM Batch Simulation ===")
    print(f"Config: {args.conf}")
    print(f"Output: {args.out}")
    print(f"Input files: {len(input_files)}")
    print(f"Native NSCC configs: {len(native_nscc_config_file)}")
    print(f"PCM CC algorithms: {len(pcm_cc_config_file)}")
    print(f"Generate plots: {args.plot}")
    print(f"Generate performance plots: {args.profile}")
    print()

    for input_file in input_files:
        file_name = Path(input_file.split()[0]).stem
        input_file = input_file.split()
        print(f"input_file: {input_file}")

        # Run native NSCC with native configuration
        if run_without_pcm:
            cmd = [binary, '-goal'] + input_file + htsim_params + baseline_config
            test_name = f"{file_name}_native_nscc_native_config"
            success = run_command(cmd, output_dir, test_name, create_plot=args.plot)
            results.append((test_name, success))
            print()
        
        # Run native NSCC with user defined configurations
        for nscc_config in native_nscc_config_file:
            test_name = f"{file_name}_native_nscc_{Path(nscc_config).stem}"
            nscc_config = nscc_config.split()
            cmd = [binary, '-goal'] + input_file + htsim_params + baseline_config + nscc_config
            success = run_command(cmd, output_dir, test_name, create_plot=args.plot)
            results.append((test_name, success))
            print()
        
        # Run PCM CC configurations
        for pcm_cc_config in pcm_cc_config_file:
            test_name = f"{file_name}_pcm_cc_{Path(pcm_cc_config).stem}"
            pcm_cc_config = pcm_cc_config.split()
            cmd = [binary, '-goal'] + input_file + htsim_params + baseline_config + pcm_delay_config + pcm_cc_config
            
            success = run_command(cmd, output_dir, test_name, create_plot=args.plot)
            results.append((test_name, success))
            print()
    
    # Summary
    total = len(results)
    passed = sum(1 for _, success in results if success)
    failed = total - passed
    
    print("=== Summary ===")
    print(f"Total simulations: {total}")
    print(f"Successful: {passed}")
    print(f"Failed: {failed}")
    print(f"Native NSCC configs: {len(native_nscc_config_file)}")
    print(f"PCM CC algorithms: {len(pcm_cc_config_file)}")
    
    if failed > 0:
        print("\nFailed simulations:")
        for name, success in results:
            if not success:
                print(f"  - {name}")
    
    # Save summary
    summary = {
        'timestamp': datetime.now().isoformat(),
        'config_file': str(args.conf),
        'total_simulations': total,
        'successful': passed,
        'failed': failed,
        'native_nscc_config_tested': len(native_nscc_config_file),
        'pcm_cc_config_tested': len(pcm_cc_config_file),
        'input_files': len(input_files),
        'results': [{'name': name, 'success': success} for name, success in results]
    }
    
    with open(output_dir / 'summary.json', 'w') as f:
        json.dump(summary, f, indent=2)
        f.write('\n')  # Add trailing newline
    
    # Generate performance plots if requested
    if args.profile:
        print()
        generate_performance_plot("TRIGGER CYCLE", "trigger_cycle_perf.pdf", output_dir)
  
    print()  # Final newline for clean terminal output
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
