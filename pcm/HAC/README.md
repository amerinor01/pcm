This is the handler compiler implementation (implemented as LLVM passes).

The pass is a modern LLVM pass (needs LLVM 14).

To compile the passes:

```
mkdir build
cd build
cmake ..
make
```

All handlers are compiled into llvm ir in the lib folder, named ```*.ll```
They can then be optimised using (for example):

```
opt -load-pass-plugin ./ReplaceCallWithLoad.so -passes=replace-call-with-load -S ../../lib/libswift.ll -o output.ll
```
