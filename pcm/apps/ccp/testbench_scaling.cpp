// Complete testbench for PCM VM scaling
// - Loads PCM algorithm shared library directly
// - Starts synthetic connections
// - Generates ACKs
// - Receives congestion control decisions via PCM VM interface
//
// for take in {1,2,3,4,5}; do for i in {1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,16384,32768,65536,131072,262144,524288,1048576};
// do ./testbench_scaling 10000000 $i 42 0 nscc_v2 1 &> ../testbench-submit-flow-nscc/${i}.${take}.nscc_v2.log ; done; done
// clang++ -std=c++23 -O3 -o testbench_scaling testbench_scaling.cpp -I../pcm/include -lpthread -ldl -DTIMING_BACKEND=RDTSC -DPROFILE_ASYNC_INVOKE=y

#include <string>
#include <atomic> // for std::atomic in PcmVmContext
#include <chrono>
#include <csignal>
#include <cstddef> // for offsetof
#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <sys/syscall.h>
#include <thread> // for sleep_for and std::thread
#include <vector>
#include <memory>
#include <algorithm>
#include <numeric>

// Thread mapping
#include <pthread.h>
#include <sched.h>

// RDTSC
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

// Profiling modes
// #define PROFILE_SUBMIT 1
// #define PROFILE_ASYNC_INVOKE 1
// #define PROFILE_FULL_CYCLE 1

#define ENABLE_TIMING_LIB 1
#include "../pcm/include/timing.hpp"
// main thread timing
#ifdef PERF_INSTR_TIMER
using timing_backend = timing_lib::PerfInstrTimer;
#elif defined(CHRONO_TIMER)
using timing_backend = timing_lib::ChronoTimer;
#else
using timing_backend = timing_lib::RdtscTimer;
#endif
timing_backend stats{
    timing_lib::benchmark_timer<timing_backend>("main thread timer")};
timing_backend timing{stats};

// PCM includes
#include "../pcm/include/pcm_vm.hpp"

using u64 = uint64_t;
using u32 = uint32_t;
using namespace std::chrono;

const int EVENTS_PER_FLOW = 32;

// ---------- Event structure ----------
struct NetworkEvent
{
    enum Type
    {
        ACK,  // Packet acknowledged
        LOSS, // Packet lost
    } type;

    u32 rtt_sample_us;     // RTT sample (only for ACK events)
    bool ecn_marked;       // Was this packet ECN marked? (only for ACK events)
    u32 packets_in_flight; // Current inflight count
    u32 packet_size;       // Packet size in bytes (MSS)
};

// ---------- Event trace generation ----------
std::vector<NetworkEvent> generate_event_trace(int num_events, uint64_t seed,
                                               bool no_loss)
{
    std::vector<NetworkEvent> events;
    events.reserve(num_events);

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> rtt_jitter_us(1, 20);
    std::bernoulli_distribution loss_ber(no_loss ? 0.0 : 0.2);
    std::bernoulli_distribution ecn_ber(0.5);
    std::uniform_int_distribution<int> inflight_pkts(50, 1500);

    const u32 mss = 1460; // Maximum segment size

    for (int i = 0; i < num_events; i++)
    {
        NetworkEvent evt;
        evt.packet_size = mss;

        if (loss_ber(rng))
        {
            // Packet loss event
            evt.type = NetworkEvent::LOSS;
            evt.rtt_sample_us = 0;
            evt.ecn_marked = false;
        }
        else
        {
            // Single packet ACK event (with optional ECN marking)
            evt.type = NetworkEvent::ACK;
            evt.rtt_sample_us = (u32)rtt_jitter_us(rng);
            evt.ecn_marked = ecn_ber(rng);
        }

        evt.packets_in_flight = (u32)inflight_pkts(rng);
        events.push_back(evt);
    }

    return events;
}

// ============================================================================
// PCM VM Context
// ============================================================================

struct PcmFlowContext
{
    pcm_vm::PcmHandlerVmDesc *vm;
    pcm_vm::PcmHandlerVmDesc::PcmHandlerVmIoSlab *io_slab;
    ~PcmFlowContext()
    {
        delete vm;
    }
};

struct alignas(64) PaddedFlowPtr
{
    std::unique_ptr<PcmFlowContext> ptr;
};

struct alignas(64) PaddedTrace
{
    std::vector<NetworkEvent> trace;
};

struct PcmVmContext
{
    std::size_t events_per_flow;
    std::vector<PaddedFlowPtr> flows;
    std::vector<PaddedTrace> flow_traces;
    std::thread handler_thread;
    std::vector<std::thread> datapath_threads;
    size_t to_process;
    size_t datapath_warmup{10000};
    size_t handler_warmup{100000};
    alignas(64) std::atomic<bool> worker_finished{false};
};

static void pcm_vm_submit_event(PcmVmContext *pctx, const NetworkEvent &evt, int flow_id);

// Handler thread function - runs invoke_cc_algorithm_on_trigger() in tight loop
static void pcm_vm_datapath_thread_fn(PcmVmContext *pctx, size_t thread_idx)
{
    std::string thread_name = "[datapath thread " + std::to_string(thread_idx) + "]";
    // usleep(10000000); // sleep to see TID and attach perf manually
    timing_backend thread_timing_stats{
        timing_lib::benchmark_timer<timing_backend>(thread_name)};
    timing_backend thread_timing{thread_timing_stats};

    size_t warmup = pctx->datapath_warmup;
    size_t events_per_flow = pctx->events_per_flow;
    size_t num_flows = pctx->flows.size();

    auto event_trace = generate_event_trace(1, 42, 1);

    // Pre-generate random flow order
    std::vector<int>
        flow_indices(num_flows);
    for (size_t i = 0; i < num_flows; ++i)
        flow_indices[i] = i;
    std::mt19937 rng(thread_idx + 0x12345678);
    std::shuffle(flow_indices.begin(), flow_indices.end(), rng);

    std::cout << thread_name << ": Started. TID=" << syscall(SYS_gettid) << std::endl;
    std::cout << thread_name << " warmup=" << warmup << " evts_per_flow=" << events_per_flow << std::endl;

    size_t perm_idx = 0;
    int evt_idx = 0;
    size_t submitted = 0;

    while (!pctx->worker_finished.load(std::memory_order_acquire))
    {
        if (submitted == warmup)
        {
            thread_timing.start();
        }

        int flow_idx = flow_indices[perm_idx];
        pcm_vm_submit_event(pctx, event_trace[0], flow_idx);

        perm_idx++;
        if (perm_idx >= num_flows)
        {
            perm_idx = 0;
            evt_idx = (evt_idx + 1) % events_per_flow;
        }
        ++submitted;
    }

    if (submitted > warmup)
    {
        auto measurement = thread_timing.stop(true, thread_name);
        auto tot_submitted = submitted - warmup;
        std::cout << thread_name << ": submitted=" << tot_submitted << " time_per_sub=" << std::get<1>(measurement) / tot_submitted << "ns" << std::endl;
    }
    else
    {
        std::cout << thread_name << ": submitted=" << submitted << " (warmup not completed)" << std::endl;
    }
}

// Handler thread function - runs invoke_cc_algorithm_on_trigger() in tight loop
static void pcm_vm_handler_thread_fn(PcmVmContext *pctx)
{
    // std::cout << "[handler thread]: Started. TID=" << syscall(SYS_gettid)
    //           << "\n";
    //  usleep(10000000); // sleep to see TID and attach perf manually
    timing_backend worker_timing_stats{
        timing_lib::benchmark_timer<timing_backend>("worker thread timer")};
    timing_backend worker_timing{worker_timing_stats};
    size_t processed = 0;
    bool timing_started = false;

    while (processed < pctx->to_process)
    {
        if (!timing_started && processed >= pctx->handler_warmup)
        {
            worker_timing.start();
            timing_started = true;
        }
        for (auto &flow : pctx->flows)
        {
            if (flow.ptr->vm->invoke_cc_algorithm_on_trigger())
            {
                ++processed;
            }
        }
    }
    auto measurements = worker_timing.stop(true, "[worker timing]");
    auto final_processed = processed - pctx->handler_warmup;
    std::cout << "[worker stats]: processed=" << final_processed << " time_per_run=" << std::get<1>(measurements) / final_processed << "ns" << std::endl;
    usleep(100);
    pctx->worker_finished.store(true, std::memory_order_release);
}

static void pcm_vm_submit_event(PcmVmContext *pctx, const NetworkEvent &evt, int flow_id)
{
    auto &flow = pctx->flows[flow_id];
    auto &slab_in = flow.ptr->io_slab->in;
    auto &vm = flow.ptr->vm;

#if defined(PROFILE_SUBMIT)
    timing.start();
#endif
    if (evt.type == NetworkEvent::LOSS)
    {
        // Packet loss event
        slab_in.nack = 1;
        slab_in.data_nacked = evt.packet_size;
        slab_in.mask |= (1 << PCM_SIG_NACK) | (1 << PCM_SIG_DATA_NACKED);
    }
    else
    {
        // Single packet ACK (with optional ECN marking)
        slab_in.ack = 1;
        slab_in.ecn = evt.ecn_marked ? 1 : 0;
        slab_in.data_tx = evt.packet_size;
        slab_in.rtt = evt.rtt_sample_us;
        slab_in.mask |= (1 << PCM_SIG_ACK) |
                        (evt.ecn_marked ? (1 << PCM_SIG_ECN) : 0) |
                        (1 << PCM_SIG_DATA_TX) | (1 << PCM_SIG_RTT);
    }

    // Common fields
    slab_in.in_flight = evt.packets_in_flight * (u64)evt.packet_size;
    slab_in.tx_backlog_bytes = 8 * 1024 * 1024; // Same as libccp example
    slab_in.tx_ready_pkts = 1;                  // update backlog (for LB)
    slab_in.mask |= (1 << PCM_SIG_IN_FLIGHT) | (1 << PCM_SIG_TX_BACKLOG_BYTES) |
                    (1 << PCM_SIG_TX_READY_PKTS);

    // Flush the input to the VM
    vm->flush_slab_input();
#ifdef PROFILE_SUBMIT
    timing.stop(true, "[submit->invoke]");
#endif
}

// Create PCM VM context
// algo_name: algorithm shared library name (e.g., "uec_dctcp")
std::unique_ptr<PcmVmContext> create_pcm_vms(const char *algo_name, int nflows, int events_per_flow, int iters)
{
    // Load the algorithm's factory function
    std::string lib_name = std::string("lib") + algo_name + "_spec.so";
    std::string factory_fn = std::string("__") + algo_name + "_atomic_spec_get";

    std::cout << "[PCM] Attempting to load library: " << lib_name << "\n";
    std::cout << "[PCM] Looking for factory function: " << factory_fn << "\n";

    auto result = pcm_vm::util::shared_symbol_open(lib_name, factory_fn);
    if (!result.has_value())
    {
        std::cerr << "[PCM] Failed to load PCM algorithm library: " << lib_name
                  << "\n";
        std::cerr << "[PCM] Make sure the library is in LD_LIBRARY_PATH or "
                     "same directory\n";
        throw std::runtime_error("Failed to load PCM algorithm: " + lib_name);
    }

    auto [so_handle, raw_fn_ptr] = result.value();
    auto factory =
        reinterpret_cast<pcm_vm::PcmHandlerVmDesc *(*)()>(raw_fn_ptr);

    std::cout << "[PCM] Successfully loaded library and factory function\n";
    std::cout << "[PCM] Mode: async\n";

    // Setup context
    auto pctx = std::make_unique<PcmVmContext>();
    pctx->events_per_flow = events_per_flow;
    pctx->to_process = iters;
    pctx->worker_finished.store(false, std::memory_order_release);

    for (int i = 0; i < nflows; i++)
    {
        // Create VM instance
        // std::cout << "[PCM] Creating VM instance " << i << "...\n";
        auto *vm = factory();
        // std::cout << "[PCM] VM instance created successfully\n";

        auto *io_slab = vm->get_signal_io_slab();

        vm->add_get_time_source(
            []() -> pcm_uint
            { return (pcm_uint)(__rdtsc() / 2.3); });

        auto flow = std::unique_ptr<PcmFlowContext>(new PcmFlowContext());
        flow->vm = vm;
        flow->io_slab = io_slab;
        pctx->flows.push_back(PaddedFlowPtr{std::move(flow)});
    }

    std::cout << "[PCM] Context created successfully\n";
    return pctx;
}

// ---------- Main ----------
int main(int argc, char **argv)
{
    if (argc > 1 &&
        (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help"))
    {
        std::cout << "Usage: " << argv[0]
                  << " [iters] [nflows] [seed] [no_loss] [algo] [num_threads]\n"
                  << "  iters: number of iterations (default: 100)\n"
                  << "  nflows: number of flows (default: 1)\n"
                  << "  seed: random seed (default: 0xC0FFEE)\n"
                  << "  no_loss: 0 or 1 (default: 0)\n"
                  << "  algo: algorithm name (default: reno)\n"
                  << "  num_threads: number of datapath threads (default: 1)\n"
                  << "\nExamples:\n"
                  << "  PCM async mode:  " << argv[0]
                  << " 1000 10 42 0 dcqcn 4\n";
        return 0;
    }

    int iters = (argc > 1) ? std::max(1, std::atoi(argv[1])) : 100;
    int nflows = (argc > 2) ? std::atoi(argv[2]) : 1;
    uint64_t seed = (argc > 3) ? (uint64_t)std::stoull(argv[3]) : 0xC0FFEE;
    bool no_loss = (argc > 4) ? (std::atoi(argv[4]) != 0) : false;
    std::string algo = (argc > 5) ? argv[5] : "reno";
    int num_threads = (argc > 6) ? std::atoi(argv[6]) : 1;

    // --- Generate random events upfront ---
    std::cout << "[main] Generating " << iters
              << " random network events (seed=" << seed
              << " no_loss=" << no_loss << ")...\n";

    std::vector<std::vector<NetworkEvent>> flow_traces;

    // --- PCM VM mode ---
    std::cout << "[main] Using PCM VM mode with algorithm: " << algo
              << "\n";
    auto pctx = create_pcm_vms(algo.c_str(), nflows, EVENTS_PER_FLOW, iters);
    pctx->flow_traces.reserve(nflows);
    for (int i = 0; i < nflows; ++i)
    {
        pctx->flow_traces.push_back(PaddedTrace{generate_event_trace(EVENTS_PER_FLOW, seed + i, no_loss)});
    }

    std::cout << "[main] Running " << iters << " events (seed=" << seed
              << ")\n";

    // Start handler thread
    pctx->handler_thread =
        std::thread(pcm_vm_handler_thread_fn, pctx.get());
    std::cout << "[PCM] Handler thread started\n";
#ifdef __linux__
    // Pin handler thread to core 49 (sibling core) through native handle
    // from std::thread
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(48, &cpuset); // optimal mapping for Intel(R) Xeon(R) Gold 6140 CPU @ 2.30GHz
    pthread_setaffinity_np(pctx->handler_thread.native_handle(),
                           sizeof(cpu_set_t), &cpuset);
#endif

    // Start datapath threads
    for (int i = 0; i < num_threads; ++i)
    {
        pctx->datapath_threads.emplace_back(pcm_vm_datapath_thread_fn, pctx.get(), i);
#ifdef __linux__
        // Pin handler thread to core 49 (sibling core) through native handle
        // from std::thread
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(13 + i, &cpuset); // optimal mapping for Intel(R) Xeon(R) Gold 6140 CPU @ 2.30GHz
        pthread_setaffinity_np(pctx->datapath_threads[i].native_handle(),
                               sizeof(cpu_set_t), &cpuset);
#endif
    }

    // Wait for datapath threads
    for (auto &t : pctx->datapath_threads)
    {
        t.join();
    }

    // Wait for handler thread
    if (pctx->handler_thread.joinable())
    {
        pctx->handler_thread.join();
    }

    return 0;
}