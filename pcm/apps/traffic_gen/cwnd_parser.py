import re
import matplotlib.pyplot as plt
from datetime import datetime
import sys

def parse_log(file_path):
    """
    Parse the log file, extracting cwnd values and their associated timestamps (or line index as fallback).
    Returns two lists: times and cwnd_values.
    """
    cwnd_vals = []
    times = []
    cwnd_pattern = re.compile(r'cwnd=(\d+)')
    # Timestamp pattern: adjust if logs include timestamps like [2025-06-16 12:34:56]
    ts_pattern = re.compile(r'^\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\]')
    
    with open(file_path, 'r') as f:
        for idx, line in enumerate(f):
            m = cwnd_pattern.search(line)
            if not m:
                continue
            cwnd = int(m.group(1))
            cwnd_vals.append(cwnd)
            ts_match = ts_pattern.search(line)
            if ts_match:
                t = datetime.strptime(ts_match.group(1), '%Y-%m-%d %H:%M:%S')
            else:
                # Fallback to line index if no timestamp present
                t = idx
            times.append(t)
    
    return times, cwnd_vals


def plot_cwnd(times, cwnd_vals):
    """
    Plot cwnd over time using matplotlib.
    """
    plt.figure(figsize=(10, 6))
    plt.plot(times, cwnd_vals)
    plt.xlabel('Time')
    plt.ylabel('cwnd')
    plt.title('Congestion Window Evolution')
    plt.tight_layout()
    plt.show()


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python parser.py <log_file>")
        sys.exit(1)
    
    log_file = sys.argv[1]
    times, cwnd_vals = parse_log(log_file)
    plot_cwnd(times, cwnd_vals)
