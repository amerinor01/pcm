import re
import matplotlib.pyplot as plt
import sys

def parse_log(file_path):
    """
    Parse the log file, extracting TIME and CWND values for each flow address.
    Returns a dict: {flow_addr: (times, cwnd_vals)}
    """
    flow_data = {}
    # Pattern for [PCM flow=0x<hex_address>, ACK]: TIME=... CWND=...
    pattern = re.compile(r'\[PCM flow=(0x[0-9a-fA-F]+), ACK\]: TIME=(\d+) CWND=(\d+)')

    with open(file_path, 'r') as f:
        for line in f:
            match = pattern.search(line)
            if match:
                flow_addr = match.group(1)  # hex address as string
                smartt_time = int(match.group(2))
                cwnd = int(match.group(3))
                if flow_addr not in flow_data:
                    flow_data[flow_addr] = ([], [])
                flow_data[flow_addr][0].append(smartt_time)
                flow_data[flow_addr][1].append(cwnd)
    return flow_data


def plot_cwnd(flow_data, out_file):
    """
    Plot absolute value of cwnd over PCM TIME for each flow using matplotlib.
    """
    plt.figure(figsize=(12, 8))
    for flow_addr, (times, cwnds) in flow_data.items():
        abs_cwnds = [abs(c) for c in cwnds]
        print(len(times))
        plt.plot(times, abs_cwnds, label=f'Flow {flow_addr}')
    plt.xlabel('Simulated time [ps]')
    plt.ylabel('Congestion window [Bytes]')
    plt.title('Congestion Window Evolution')
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
