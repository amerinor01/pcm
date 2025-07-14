import re
import matplotlib.pyplot as plt
import sys

def parse_log(file_path):
    """
    Parse the log file, extracting PCM TIME and cwnd values for each flow id.
    Returns a dict: {flow_id: (smartt_times, cwnd_vals)}
    """
    flow_data = {}
    # Pattern for [flow_id:...] PCM TIME=... cwnd=...
    pattern = re.compile(r'\[(\d+):[\d]+\] PCM TIME=(\d+) cwnd=(\d+)')

    with open(file_path, 'r') as f:
        for line in f:
            match = pattern.search(line)
            if match:
                flow_id = int(match.group(1))
                smartt_time = int(match.group(2))
                cwnd = int(match.group(3))
                if flow_id not in flow_data:
                    flow_data[flow_id] = ([], [])
                flow_data[flow_id][0].append(smartt_time)
                flow_data[flow_id][1].append(cwnd)
    return flow_data


def plot_cwnd(flow_data, out_file):
    """
    Plot absolute value of cwnd over PCM TIME for each flow using matplotlib.
    """
    plt.figure(figsize=(12, 8))
    for flow_id, (times, cwnds) in flow_data.items():
        abs_cwnds = [abs(c) for c in cwnds]
        print(len(times))
        plt.plot(times, abs_cwnds, label=f'Flow {flow_id}')
    plt.xlabel('PCM TIME')
    plt.ylabel('cwnd')
    plt.title('Congestion Window Evolution per Flow (Absolute Value)')
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_file, bbox_inches='tight')
    plt.show()


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: python cwnd_parser.py <log_file> <out_file>")
        sys.exit(1)
    
    log_file = sys.argv[1]
    out_file = sys.argv[2]
    flow_data = parse_log(log_file)
    plot_cwnd(flow_data, out_file)
