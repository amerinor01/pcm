import os
import re
import sys
import matplotlib.pyplot as plt
import seaborn as sns
from collections import defaultdict

SEARCH_DIR = sys.argv[1]

# Regex to match the performance line
perf_line_pattern = re.compile(r"HANDLER PERFORMANCE (\d+) cycles (\d+) instructions")
# Data structures
cycles_data = defaultdict(list)
instr_data = defaultdict(list)

# Search for .log files
for filename in os.listdir(SEARCH_DIR):
    if filename.endswith(".log"):
        filepath = os.path.join(SEARCH_DIR, filename)
        with open(filepath, 'r') as f:
            for line in f:
                #print(f"looking at line {line} in {filename}")
                match = perf_line_pattern.search(line)
                if match:
                    c = int(match.group(1))
                    i = int(match.group(2))
                    cycles_data[filename].append(c)
                    instr_data[filename].append(i)
                    #print(f"data {c},{i} added for {filename}")

# Sort keys to ensure consistent order
file_keys = sorted(cycles_data.keys())

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
plt.savefig(f"{SEARCH_DIR}.out.pdf", bbox_inches='tight')
plt.show()
