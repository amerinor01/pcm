import os
import re
import sys
import matplotlib.pyplot as plt
import seaborn as sns
from collections import defaultdict

if (len(sys.argv) < 4):
    print(f"Usage: {sys.argv[0]} logdir outfile profile-pattern")
    sys.exit(1)

SEARCH_DIR = sys.argv[1]
OUTFILE = sys.argv[2]
PROFILE_PATTERN = sys.argv[3]

# Regex to match the performance line
perf_line_pattern = re.compile(rf"{PROFILE_PATTERN} (\d+) cycles (\d+) instructions")
# Data structures
cycles_data = defaultdict(list)
instr_data = defaultdict(list)

# Search for *.log files in subdirectories
for item in os.listdir(SEARCH_DIR):
    item_path = os.path.join(SEARCH_DIR, item)
    if os.path.isdir(item_path):
        print(f"Processing experiment folder: {item}")
        # Look for all .log files in this experiment folder
        log_files = [f for f in os.listdir(item_path) if f.endswith('.log')]
        for log_file in log_files:
            log_path = os.path.join(item_path, log_file)
            print(f"  Checking: {log_path}")
            with open(log_path, 'r') as f:
                for line in f:
                    #print(f"looking at line {line} in {item}")
                    match = perf_line_pattern.search(line)
                    if match:
                        c = int(match.group(1))
                        i = int(match.group(2))
                        cycles_data[item].append(c)
                        instr_data[item].append(i)
                        print(f"    Found data in {log_file}: {c} cycles, {i} instructions")

print(f"Found data for {len(cycles_data)} algorithms")
for key in sorted(cycles_data.keys()):
    print(f"  {key}: {len(cycles_data[key])} data points")

# Sort keys to ensure consistent order
file_keys = sorted(cycles_data.keys())

# Check if we found any data
if not file_keys:
    print("No performance data found!")
    print(f"Searched in: {SEARCH_DIR}")
    print(f"Make sure log files contain lines matching:")
    print(f"{PROFILE_PATTERN} <cycles> cycles <instructions> instructions")
    sys.exit(1)

# Prepare data for violin plot
cycle_violins = [cycles_data[key] for key in file_keys]
instr_violins = [instr_data[key] for key in file_keys]

# Plot
sns.set(style="whitegrid")
fig, axes = plt.subplots(nrows=2, ncols=1, figsize=(12, 8), sharex=True)

# Cycles plot
sns.violinplot(data=cycle_violins, ax=axes[0])
axes[0].set_title("CPU Cycles")
axes[0].set_ylabel("Cycles")
axes[0].set_xticklabels(file_keys, rotation=45, ha="right")

# Instructions plot
sns.violinplot(data=instr_violins, ax=axes[1])
axes[1].set_title("Instructions")
axes[1].set_ylabel("Instructions")
axes[1].set_xlabel("CM Algorithm")
axes[1].set_xticklabels(file_keys, rotation=45, ha="right")

plt.tight_layout()
plt.savefig(OUTFILE, bbox_inches='tight')
print(f"Plot saved in {OUTFILE}")
plt.show()
