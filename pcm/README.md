# Programmable Congestion Management (PCM) prototype implementation

![](pcm_arch.png)

## Project structure
- `./include` - contains API definitions of PCM and abstract NIC that supports PCM
- `./src` - contains implementation of the PCM and NIC APIs
- `./algos` - contains examples of PCM-based congestion control algorithms: NewReno, DCTCP, DCQCN, Swift, SMaRTT
- `./app` - contains application that runs N synthetic flows served by a given CC algo

## Running synthetic application

1. `make` - compiles whole project (by default with `clang`) into `lib` (app binary) and `bin` folders
2. `export LD_LIBRARY_PATH=$(pwd):$LD_LIBRARY_PATH`
3. `$./bin/app_main 1 10000000 dctcp $(pwd)/lib/libdctcp.so &> dctcp.log` runs single flow for 10 seconds (10000000 us) and outputs log into the `dctcp.log` file
4. `python3 ./app/parser.py dctcp.log` - parses the from the previous steplog and outputs congestion window evolution plot on the screen:
![](dctcp_cwnd.png)