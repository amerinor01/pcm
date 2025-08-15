#!/usr/bin/env python3

"""
HTSIM PCM Congestion Window Parser and Plotter
Parses log files and generates CWND evolution plots.

Usage: python3 htsim_pcm_cwnd_parser.py <log_file> <output_plot>
"""

import argparse
import re
import sys
from pathlib import Path
from typing import Dict, List, Tuple

try:
    import matplotlib
    matplotlib.use('Agg')  # Use non-interactive backend for server environments
    import matplotlib.pyplot as plt
except ImportError:
    print("Error: matplotlib is required. Install with: pip install matplotlib")
    sys.exit(1)


def parse_log_file(file_path: Path) -> Tuple[Dict[str, Tuple[List[int], List[int]]], Dict[str, int]]:
    """
    Parse the log file, extracting TIME and CWND values for each flow address,
    and BDP information.
    
    Args:
        file_path: Path to the log file to parse
        
    Returns:
        Tuple of (flow_data, bdp_info) where:
        - flow_data: Dictionary mapping flow addresses to (times, cwnd_values) tuples
        - bdp_info: Dictionary containing 'network_bdp', 'flow_bdp', 'flow_maxwnd'
    """
    if not file_path.exists():
        raise FileNotFoundError(f"Log file not found: {file_path}")
    
    flow_data = {}
    bdp_info = {'network_bdp': None, 'flow_bdp': None, 'flow_maxwnd': None}
    
    # Pattern for [PCM flow=0x<hex_address>, ACK]: TIME=... CWND=...
    pattern = re.compile(r'\[PCM flow=(0x[0-9a-fA-F]+), ACK\]: TIME=(\d+) CWND=(\d+)')
    
    # BDP patterns
    network_bdp_pattern = re.compile(r'_network_bdp=(\d+)')
    flow_params_pattern = re.compile(
        r'Initialize per-instance NSCC parameters: flowid (\d+) _base_rtt=(\d+) _base_bdp=(\d+) _bdp=(\d+) _min_cwnd=(\d+) _maxwnd=(\d+) _cwnd=(\d+)'
    )
    
    try:
        with open(file_path, 'r') as f:
            for line_num, line in enumerate(f, 1):
                try:
                    # Extract network BDP
                    if bdp_info['network_bdp'] is None:
                        network_bdp_match = network_bdp_pattern.search(line)
                        if network_bdp_match:
                            bdp_info['network_bdp'] = int(network_bdp_match.group(1))
                    
                    # Extract per-flow parameters (take first flow found)
                    if bdp_info['flow_bdp'] is None:
                        flow_params_match = flow_params_pattern.search(line)
                        if flow_params_match:
                            flowid, base_rtt, base_bdp, bdp, min_cwnd, maxwnd, init_cwnd = flow_params_match.groups()
                            bdp_info['flow_bdp'] = int(bdp)
                            bdp_info['flow_maxwnd'] = int(maxwnd)
                    
                    # Extract PCM flow data
                    match = pattern.search(line)
                    if match:
                        flow_addr = match.group(1)  # hex address as string
                        pcm_time = int(match.group(2))
                        cwnd = int(match.group(3))
                        
                        if flow_addr not in flow_data:
                            flow_data[flow_addr] = ([], [])
                        
                        flow_data[flow_addr][0].append(pcm_time)
                        flow_data[flow_addr][1].append(cwnd)
                        
                except ValueError as e:
                    print(f"Warning: Could not parse line {line_num}: {e}")
                    continue
                    
    except Exception as e:
        raise RuntimeError(f"Error reading log file {file_path}: {e}")
    
    if not flow_data:
        print(f"Warning: No PCM flow data found in {file_path}")
    
    return flow_data, bdp_info


def create_cwnd_plot(flow_data: Dict[str, Tuple[List[int], List[int]]], 
                    bdp_info: Dict[str, int],
                    output_path: Path, 
                    title: str = "Congestion Window Evolution") -> bool:
    """
    Create and save a congestion window evolution plot with BDP reference lines.
    
    Args:
        flow_data: Dictionary of flow data from parse_log_file
        bdp_info: Dictionary containing BDP information
        output_path: Path where to save the plot
        title: Plot title
        
    Returns:
        True if plot was created successfully, False otherwise
    """
    if not flow_data:
        print("No flow data to plot")
        return False
    
    try:
        plt.figure(figsize=(12, 8))
        
        # Set font size for all plot elements
        plt.rcParams.update({'font.size': 14})
        
        for flow_addr, (times, cwnds) in flow_data.items():
            if not times or not cwnds:
                print(f"Warning: Empty data for flow {flow_addr}")
                continue
                
            # Take absolute value to handle negative CWND values
            abs_cwnds = [abs(c) for c in cwnds]
            
            plt.plot(times, abs_cwnds, label=f'Flow {flow_addr}', linewidth=1.5)
            print(f"Plotted {len(times)} points for flow {flow_addr}")
        
        # Add BDP reference lines
        # if bdp_info['flow_bdp'] is not None:
        #     plt.axhline(y=bdp_info['flow_bdp'], color='red', linestyle='--', alpha=0.8, 
        #                label=f'Flow BDP ({bdp_info["flow_bdp"]} bytes)')
        
        # if bdp_info['flow_maxwnd'] is not None:
        #     plt.axhline(y=bdp_info['flow_maxwnd'], color='orange', linestyle=':', alpha=0.8,
        #                label=f'Max Window ({bdp_info["flow_maxwnd"]} bytes)')
        
        # if bdp_info['network_bdp'] is not None and bdp_info['network_bdp'] != bdp_info['flow_bdp']:
        #     plt.axhline(y=bdp_info['network_bdp'], color='purple', linestyle='-.', alpha=0.8,
        #                label=f'Network BDP ({bdp_info["network_bdp"]} bytes)')
        
        plt.xlabel('Simulated Time [ps]')
        plt.ylabel('Congestion Window [Bytes]')
        plt.title(title)
        plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
        plt.grid(True, alpha=0.3)
        plt.tight_layout()
        
        # Ensure output directory exists
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()  # Free memory
        
        print(f"Plot saved to: {output_path}")
        return True
        
    except Exception as e:
        print(f"Error creating plot: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Parse HTSIM PCM logs and generate CWND plots",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 htsim_pcm_cwnd_parser.py --input=stdout.log --output=cwnd_plot.png
  python3 htsim_pcm_cwnd_parser.py --input=log.txt --output=plot.pdf --title="NewReno CWND"
        """
    )
    
    parser.add_argument('--input', required=True, type=Path, help='Input log file to parse')
    parser.add_argument('--output', required=True, type=Path, help='Output plot file (.png, .pdf, .svg)')
    parser.add_argument('--title', default='Congestion Window Evolution',
                       help='Plot title')
    
    args = parser.parse_args()
    
    try:
        # Parse log file
        print(f"Parsing log file: {args.input}")
        flow_data, bdp_info = parse_log_file(args.input)
        
        if not flow_data:
            print("No PCM flow data found in log file")
            return 1
        
        print(f"Found {len(flow_data)} flows")
        
        # Print BDP info if available
        if bdp_info['flow_bdp'] is not None:
            print(f"Flow BDP: {bdp_info['flow_bdp']} bytes")
        if bdp_info['network_bdp'] is not None:
            print(f"Network BDP: {bdp_info['network_bdp']} bytes")
        if bdp_info['flow_maxwnd'] is not None:
            print(f"Flow Max Window: {bdp_info['flow_maxwnd']} bytes")
        
        # Create plot
        success = create_cwnd_plot(flow_data, bdp_info, args.output, args.title)
        
        return 0 if success else 1
        
    except Exception as e:
        print(f"Error: {e}")
        return 1


if __name__ == '__main__':
    sys.exit(main())
