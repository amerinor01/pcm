#!/bin/bash

# Build the pass
mkdir -p build
cd build
cmake ..
make

# Compile test file to LLVM IR
clang -emit-llvm -S -O3 ../test.c -o test.ll

# Run the pass
opt-14 -load-pass-plugin=./enumerate_paths.so -passes="enumerate-paths" ./test.ll -disable-output

cd ..