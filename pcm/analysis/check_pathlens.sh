for f in `ls ../lib/*.ll`; do
  echo "Getting path length histogram for $f:"
  opt -load-pass-plugin ../HAC/build/EnumeratePaths.so -passes=enumerate-paths -S $f -o foo
done
