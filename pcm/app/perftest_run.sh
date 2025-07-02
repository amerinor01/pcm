#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <so_dir> <output_dir>"
  echo "Example: $0 \$(pwd)/lib /path/to/logs"
  exit 1
fi

SO_DIR=$1
OUT_DIR=$2

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
  ./bin/app_main 1 10000000 "$proto" "$SO_DIR/lib${proto}.so" \
    &> "$OUT_DIR/${proto}.prof.log"
done

# handle newreno’s two variants
./bin/app_main 1 10000000 newreno "$SO_DIR/libreno_accumulated.so" \
  &> "$OUT_DIR/reno_accum.prof.log"

./bin/app_main 1 10000000 newreno "$SO_DIR/libreno_standard.so" \
  &> "$OUT_DIR/reno_standard.prof.log"