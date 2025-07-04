#!/usr/bin/env bash
set -euo pipefail


if [ "$#" -ne 3 ]; then
  echo "Usage: $0 <traffic_gen_app> <so_dir> <output_dir>"
  echo "Example: $0 \$(pwd)/bin/traffic_gen_app \$(pwd)/lib /path/to/logs"
  exit 1
fi

TRAFFIC_GEN=$1
SO_DIR=$2
OUT_DIR=$3

# ensure the .so directory exists
if [ ! -d "$SO_DIR" ]; then
  echo "Error: SO directory '$SO_DIR' not found."
  exit 1
fi

# create output directory if needed
mkdir -p "$OUT_DIR"

# turn on command tracing
set -x

# run all but newreno in a loop
for proto in dctcp dcqcn swift smartt; do
  ${TRAFFIC_GEN} 1 10000000 "$proto" "$SO_DIR/lib${proto}.so" \
    &> "$OUT_DIR/${proto}.prof.log"
done

# handle newreno’s two variants
${TRAFFIC_GEN} 1 10000000 newreno "$SO_DIR/libreno_accumulated.so" \
  &> "$OUT_DIR/reno_accum.prof.log"

${TRAFFIC_GEN} 1 10000000 newreno "$SO_DIR/libreno_standard.so" \
  &> "$OUT_DIR/reno_standard.prof.log"


#now run the optimized handlers for comparison
# run all but newreno in a loop
for proto in dctcp dcqcn swift smartt; do
  ${TRAFFIC_GEN} 1 10000000 "$proto" "$SO_DIR/lib${proto}.ll.so" \
    &> "$OUT_DIR/${proto}_opt.prof.log"
done

# handle newreno’s two variants
${TRAFFIC_GEN} 1 10000000 newreno "$SO_DIR/libreno_accumulated.ll.so" \
  &> "$OUT_DIR/reno_accum_opt.prof.log"

${TRAFFIC_GEN} 1 10000000 newreno "$SO_DIR/libreno_standard.ll.so" \
  &> "$OUT_DIR/reno_standard_opt.prof.log"
