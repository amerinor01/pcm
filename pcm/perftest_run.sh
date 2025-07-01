set -x 
./bin/app_main 1 10000000 dctcp $(pwd)/lib/libdctcp.so > dctcp.log
./bin/app_main 1 10000000 dcqcn $(pwd)/lib/libdcqcn.so > dcqcn.log
./bin/app_main 1 10000000 swift $(pwd)/lib/libswift.so > swift.log
./bin/app_main 1 10000000 smartt $(pwd)/lib/libsmartt.so > smartt.log
./bin/app_main 1 10000000 newreno $(pwd)/lib/libreno_accumulated.so > reno_accum.log
./bin/app_main 1 10000000 newreno $(pwd)/lib/libreno_standard.so > reno_standard.log
