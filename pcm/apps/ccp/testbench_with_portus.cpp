// Complete testbench for libccp with Portus IPC support + PCM VM adapter
// - Supports two modes: libccp (with Portus IPC) and native PCM VM (SYNC mode)
// - Initializes a datapath with Unix socket IPC (libccp mode)
// - Connects to Portus userspace CCP process (libccp mode)
// - OR loads PCM algorithm shared library directly (pcm mode)
// - Starts one synthetic connection
// - Generates random ACK/ECN/loss/timeout-like events
// - Receives congestion control decisions via adapter interface
//
// Build (libccp mode):
//   clang++ -std=c++11 -o testbench_with_portus testbench_with_portus.cpp -L. -lccp -I. -lpthread
// Build (PCM mode):
//   clang++ -std=c++17 -o testbench_with_portus testbench_with_portus.cpp
//     -I../pcm-sdk/pcm/include -L../pcm-sdk/pcm/build/lib -Wl,-rpath,../pcm-sdk/pcm/build/lib
//
// Run (libccp):
//   Terminal 1: cd /path/to/generic-cong-avoid && ./target/release/reno --ipc=unix
//   Terminal 2: ./testbench_with_portus 1000 10 42 libccp reno
// Run (PCM):
//   ./testbench_with_portus 1000 10 42 pcm dcqcn

#include <chrono>
#include <atomic>  // for std::atomic in PcmVmContext
#include <cstddef> // for offsetof
#include <csignal>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <thread> // for sleep_for and std::thread
#include <sys/syscall.h>

// Unix socket includes
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>

// Thread mapping
#include <pthread.h>
#include <sched.h>

// RDTSC
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

extern "C"
{
#include "ccp.h"
}

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
using namespace std::chrono;

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
std::vector<NetworkEvent> generate_event_trace(int num_events, uint64_t seed, bool no_loss)
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
// CC Algorithm Adapter Interface
// ============================================================================

// Abstract interface for congestion control algorithms
// Allows plugging in different CC implementations (libccp, native, etc.)
struct CcAlgorithmAdapter
{
    void *context; // Opaque context (e.g., ccp_connection*, datapath state, etc.)

    // 1) Submit new event to the datapath
    // Takes the event, updates internal state
    void (*submit_event)(void *ctx, const NetworkEvent &evt);

    // 2) Invoke CC algorithm
    // Processes the event and computes new control values
    void (*invoke)(void *ctx);

    // 3) Fetch control output
    // Returns updated cwnd and rate
    struct ControlOutput
    {
        u32 cwnd{};
        u32 rate_bytes_per_s{};
        u32 ev{};
        bool skip{};
    };
    ControlOutput (*fetch_output)(void *ctx);
};

// ============================================================================
// libccp Adapter Implementation
// ============================================================================

// ---------- Libccp global state ----------
static int portus_sock = -1;           // Unix socket for IPC with Portus
static struct sockaddr_un portus_addr; // Portus socket address (for sendto)
static socklen_t portus_addr_len = 0;  // Actual length (not sizeof!)

// ---------- Timing helpers ----------
static inline u64 now_us()
{
    return (u64)duration_cast<microseconds>(
               steady_clock::now().time_since_epoch())
        .count();
}
static u64 dp_now() { return now_us(); }
static u64 dp_since_usecs(u64 then)
{
    u64 n = now_us();
    return (n > then) ? (n - then) : 0;
}
static u64 dp_after_usecs(u64 usec) { return now_us() + usec; }

// ---------- Logging ----------
static void dp_log(struct ccp_datapath *, enum ccp_log_level lvl,
                   const char *msg, int msg_size)
{
    const char *L[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR"};
    int li = (lvl >= TRACE && lvl <= ERROR) ? (int)lvl : (int)INFO;
    std::string s = (msg_size > 0) ? std::string(msg, (size_t)msg_size)
                                   : std::string(msg);
    std::cerr << "[libccp " << L[li] << "] " << s << "\n";
}

// ---------- Control hooks ----------
static void dp_set_cwnd(struct ccp_connection *c, u32 cwnd)
{
    // Update the primitives so the application can see the new value
    c->prims.snd_cwnd = cwnd;
}

static void dp_set_rate_abs(struct ccp_connection *c, u32 rate_bytes_per_s)
{
    // Update the primitives so the application can see the new value
    c->prims.snd_rate = rate_bytes_per_s;
}

// ---------- IPC: send to Portus via Unix datagram socket ----------
static int dp_send_msg(struct ccp_datapath *, char *msg, int len)
{
    if (portus_sock < 0)
    {
        std::cerr << "[IPC] Socket not connected, cannot send\n";
        return -1;
    }

    // Use sendto() with the stored Portus address
    int sent = sendto(portus_sock, msg, len, 0,
                      (struct sockaddr *)&portus_addr, portus_addr_len);
    if (sent < 0)
    {
        std::cerr << "[IPC] Failed to send message: " << strerror(errno) << "\n";
        return -1;
    }

    // std::cout << "[datapath→CCP] Sent " << sent << " bytes to Portus\n";
    return sent;
}

// ---------- Simple container ----------
struct LibccpTestbed
{
    std::vector<ccp_connection> conns;
    ccp_datapath dp{};
    ccp_connection *active_conn = nullptr;
};

// ---------- IPC receiver function ----------
// Process all pending messages from Portus
// Call this after ccp_invoke() to handle responses
int libccp_process_ipc_messages(struct ccp_datapath *dp)
{
    char buf[2048];
    struct sockaddr_un from_addr;
    socklen_t from_len;
    int messages_processed = 0;

    // Process all available messages (non-blocking)
    while (true)
    {
        from_len = sizeof(from_addr);
        int nread = recvfrom(portus_sock, buf, sizeof(buf), MSG_DONTWAIT,
                             (struct sockaddr *)&from_addr, &from_len);

        if (nread < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // No more messages available, this is normal
                break;
            }
            if (errno == EINTR)
            {
                // Interrupted, try again
                continue;
            }
            std::cerr << "[IPC] Read error: " << strerror(errno) << "\n";
            return -1;
        }

        if (nread == 0)
        {
            continue;
        }

        // Forward message to libccp for processing
        // auto t1 = std::chrono::high_resolution_clock::now();
        int ret = ccp_read_msg(dp, buf, nread);
        if (ret < 0)
        {
            std::cerr << "[IPC] ccp_read_msg failed: " << ret << "\n";
        }
        // auto t2 = std::chrono::high_resolution_clock::now();
        // std::cout << "ccp_read_msg: "
        //           << std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count()
        //           << " ns\n";

        messages_processed++;
    }

    return messages_processed;
}

// ---------- Connect to Portus via Unix socket ----------
int libccp_connect_to_portus(const char *socket_path = "/tmp/ccp/portus")
{
    std::cout << "[IPC] Connecting to Portus at " << socket_path << "...\n";

    // Unix datagram socket (not stream - Portus uses SOCK_DGRAM!)
    portus_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (portus_sock < 0)
    {
        std::cerr << "[IPC] Failed to create socket: " << strerror(errno) << "\n";
        return -1;
    }

    // Set socket timeout so recvfrom doesn't block forever
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(portus_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Bind to our own address first
    struct sockaddr_un bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sun_family = AF_UNIX;

    // Create unique socket path based on PID
    char bind_path[108];
    snprintf(bind_path, sizeof(bind_path), "/tmp/ccp/testbench%02d", getpid() % 100);
    strcpy(bind_addr.sun_path, bind_path);

    // Remove old socket if exists
    unlink(bind_path);

    // Calculate bind address length using offsetof (portable across macOS/Linux)
    // IMPORTANT: Must include the null terminator (+1) in the length!
    socklen_t bind_len = offsetof(struct sockaddr_un, sun_path) + strlen(bind_addr.sun_path) + 1;

    if (bind(portus_sock, (struct sockaddr *)&bind_addr, bind_len) < 0)
    {
        std::cerr << "[IPC] Failed to bind socket: " << strerror(errno) << "\n";
        close(portus_sock);
        portus_sock = -1;
        return -1;
    }

    // Store Portus address for sendto() later
    memset(&portus_addr, 0, sizeof(portus_addr));
    portus_addr.sun_family = AF_UNIX;
    strcpy(portus_addr.sun_path, socket_path);

    // Calculate actual address length using offsetof (portable across macOS/Linux)
    // IMPORTANT: Must include the null terminator (+1) in the length!
    portus_addr_len = offsetof(struct sockaddr_un, sun_path) + strlen(portus_addr.sun_path) + 1;

    std::cout << "[IPC] Bound to " << bind_path << "\n";
    std::cout << "[IPC] Will send to: " << socket_path
              << " (addrlen=" << portus_addr_len << ")\n";
    return 0;
}

// ---------- Datapath setup ----------
int libccp_setup_datapath(LibccpTestbed *tb)
{
    tb->conns.resize(64);

    tb->dp.set_cwnd = &dp_set_cwnd;
    tb->dp.set_rate_abs = &dp_set_rate_abs;
    tb->dp.send_msg = &dp_send_msg;
    tb->dp.log = &dp_log;
    tb->dp.now = &dp_now;
    tb->dp.since_usecs = &dp_since_usecs;
    tb->dp.after_usecs = &dp_after_usecs;
    tb->dp.time_zero = now_us();
    tb->dp.max_connections = (u32)tb->conns.size();
    tb->dp.ccp_active_connections = tb->conns.data();
    tb->dp.fto_us = 2000000; // 2 second fallback timeout
    tb->dp.max_programs = 10;
    tb->dp.programs = nullptr; // Will be allocated by ccp_init()
    tb->dp.impl = tb;

    std::cout << "[main] Initializing CCP...\n";
    if (ccp_init(&tb->dp, /*datapath_id*/ 0xDEADBEEF) < 0)
    {
        std::cerr << "[main] ccp_init failed.\n";
        return -1;
    }
    std::cout << "[main] CCP initialized!\n";

    // Give Portus time to respond to the READY message
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Process initial messages (READY response, etc.)
    libccp_process_ipc_messages(&tb->dp);

    return 0;
}

// ---------- Flow creation ----------
ccp_connection *libccp_create_flow(LibccpTestbed *tb, const char *cong_alg = "reno")
{
    ccp_datapath_info info{};
    info.init_cwnd = 10 * 1460;
    info.mss = 1460;
    info.src_ip = 0x0a000001;
    info.dst_ip = 0x0a000002;
    info.src_port = 12345;
    info.dst_port = 4433;
    strncpy(info.congAlg, cong_alg, sizeof(info.congAlg) - 1);

    std::cout << "[main] Starting connection (alg=" << cong_alg << ")...\n";
    ccp_connection *conn = ccp_connection_start(&tb->dp, nullptr, &info);
    if (!conn)
    {
        std::cerr << "[main] ccp_connection_start failed.\n";
        return nullptr;
    }

    std::cout << "[conn] Started sid=" << conn->index << "\n";

    // Give Portus time to install datapath programs
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Process CREATE response and datapath program installation
    libccp_process_ipc_messages(&tb->dp);

    tb->active_conn = conn;
    return conn;
}

// ---------- Apply event to connection primitives ----------
void libccp_apply_event(ccp_connection *conn, const NetworkEvent &evt)
{
    auto &p = conn->prims;

    if (evt.type == NetworkEvent::LOSS)
    {
        // Packet loss event
        p.packets_acked = 0;
        p.bytes_acked = 0;
        p.was_timeout = false;
        p.rtt_sample_us = 0;
        p.ecn_packets = 0;
        p.ecn_bytes = 0;
        p.lost_pkts_sample = 1;
    }
    else
    {
        // Single packet ACK (with optional ECN marking)
        p.packets_acked = 1;
        p.bytes_acked = evt.packet_size;
        p.was_timeout = false;
        p.rtt_sample_us = evt.rtt_sample_us;
        p.ecn_packets = evt.ecn_marked ? 1 : 0;
        p.ecn_bytes = evt.ecn_marked ? evt.packet_size : 0;
        p.lost_pkts_sample = 0;
    }

    p.packets_in_flight = evt.packets_in_flight;
    p.bytes_in_flight = evt.packets_in_flight * (u64)evt.packet_size;
    p.bytes_pending = 8 * 1024 * 1024;
}

struct LibccpContext
{
    LibccpTestbed *testbed;
};

// Global context for libccp adapter (since we only have one connection)
static LibccpContext g_libccp_ctx;

static void libccp_submit_event(void *ctx, const NetworkEvent &evt)
{
    auto *lctx = static_cast<LibccpContext *>(ctx);
    libccp_apply_event(lctx->testbed->active_conn, evt);
}

static void libccp_invoke(void *ctx)
{
    auto *lctx = static_cast<LibccpContext *>(ctx);

    // Let CCP process (may send measurements to Portus)
    ccp_invoke(lctx->testbed->active_conn);
}

static CcAlgorithmAdapter::ControlOutput libccp_fetch_output(void *ctx)
{
    auto *lctx = static_cast<LibccpContext *>(ctx);

    // Poll for responses from Portus until we get message or timeout
    // We expect that Portus will send an update back every ACK.
    const int max_polls = 10000;
    int polls = 0;
    bool got_response = false;

    while (polls < max_polls)
    {
        int msgs = libccp_process_ipc_messages(&lctx->testbed->dp);

        // If we got any messages from Portus, consider it responded
        if (msgs > 0)
        {
            got_response = true;
            break;
        }
        ++polls;
    }

    if (!got_response)
    {
        std::cerr << "[WARNING] Portus didn't respond within timeout (waited "
                  << polls << " polls)\n";
    }
    CcAlgorithmAdapter::ControlOutput out;
    if (lctx->testbed && lctx->testbed->active_conn)
    {
        out.cwnd = lctx->testbed->active_conn->prims.snd_cwnd;
        out.rate_bytes_per_s = lctx->testbed->active_conn->prims.snd_rate;
    }
    else
    {
        out.cwnd = 0;
        out.rate_bytes_per_s = 0;
    }
    return out;
}

// Create libccp adapter
CcAlgorithmAdapter create_libccp_adapter(LibccpTestbed *tb)
{
    g_libccp_ctx.testbed = tb;

    CcAlgorithmAdapter adapter;
    adapter.context = &g_libccp_ctx;
    adapter.submit_event = libccp_submit_event;
    adapter.invoke = libccp_invoke;
    adapter.fetch_output = libccp_fetch_output;
    return adapter;
}

// ============================================================================
// PCM VM Adapter Implementation (SYNC and ASYNC modes)
// ============================================================================
enum class PcmVmMode
{
    SYNC, // Main thread calls invoke_cc_algorithm_on_trigger directly
    ASYNC // Dedicated handler thread calls invoke_cc_algorithm_on_trigger
};

struct PcmVmContext
{
    pcm_vm::PcmHandlerVmDesc *vm;
    pcm_vm::PcmHandlerVmDesc::PcmHandlerVmIoSlab *io_slab;
    PcmVmMode mode;                                         // Operating mode
    std::thread handler_thread;                             // ASYNC mode: dedicated handler thread
    std::atomic<bool> running{true};                        // ASYNC mode: thread control flag
    alignas(64) std::atomic<uint64_t> updates_processed{0}; // ASYNC mode: incremented by handler when trigger fires
    alignas(64) uint64_t updates_fetched{0};                // ASYNC mode: main thread only, tracks last fetched update
};

// Handler thread function - runs invoke_cc_algorithm_on_trigger() in tight loop
static void pcm_vm_handler_thread_fn(pcm_vm::PcmHandlerVmDesc *vm, std::atomic<bool> *running,
                                     std::atomic<uint64_t> *updates_processed)
{
    std::cout << "[PCM Handler Thread] Started. TID=" << syscall(SYS_gettid) << "\n";
    // usleep(10000000); // sleep to see TID and attach perf manually
    // thread-local timing
    timing_backend worker_timing_stats{
        timing_lib::benchmark_timer<timing_backend>("worker thread timer")};
    timing_backend worker_timing{worker_timing_stats};

    while (running->load(std::memory_order_acquire))
    {
#ifdef PROFILE_ASYNC_INVOKE
        worker_timing.start();
#endif
        if (vm->invoke_cc_algorithm_on_trigger())
        {
#ifdef PROFILE_ASYNC_INVOKE
            worker_timing.stop(true, "[worker invocation]");
#endif
            updates_processed->fetch_add(1, std::memory_order_release);
        }
#ifdef PROFILE_ASYNC_INVOKE
        else
        {
            worker_timing.stop(false, "");
        }
#endif
    }
    std::cout << "[PCM Handler Thread] Exiting\n";
}

static void pcm_vm_submit_event(void *ctx, const NetworkEvent &evt)
{
    auto *pctx = static_cast<PcmVmContext *>(ctx);
    auto &slab_in = pctx->io_slab->in;
    auto &vm = pctx->vm;

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
        slab_in.mask |= (1 << PCM_SIG_ACK) | (evt.ecn_marked ? (1 << PCM_SIG_ECN) : 0) | (1 << PCM_SIG_DATA_TX) | (1 << PCM_SIG_RTT);
    }

    // Common fields
    slab_in.in_flight = evt.packets_in_flight * (u64)evt.packet_size;
    slab_in.tx_backlog_bytes = 8 * 1024 * 1024; // Same as libccp example
    slab_in.tx_ready_pkts = 1;                  // update backlog (for LB)
    slab_in.mask |= (1 << PCM_SIG_IN_FLIGHT) | (1 << PCM_SIG_TX_BACKLOG_BYTES) | (1 << PCM_SIG_TX_READY_PKTS);

    // Flush the input to the VM
    vm->flush_slab_input();
#ifdef PROFILE_SUBMIT
    timing.stop(true, "[submit->invoke]");
#endif
}

static void pcm_vm_invoke(void *ctx)
{
    auto *pctx = static_cast<PcmVmContext *>(ctx);
    // SYNC mode: Main thread directly invokes the trigger check
    if (pctx->mode == PcmVmMode::SYNC)
    {
        auto ret = pctx->vm->invoke_cc_algorithm_on_trigger();
        assert(ret);
    }
    // ASYNC mode: Handler thread runs invoke_cc_algorithm_on_trigger()
    // Datapath thread does nothing here - just submit events and fetch results
    // The subsequent blocking fetch will wait for handler to process the trigger
}

static CcAlgorithmAdapter::ControlOutput pcm_vm_fetch_output(void *ctx)
{
    auto *pctx = static_cast<PcmVmContext *>(ctx);
    bool skip = false;

    if (pctx->mode == PcmVmMode::ASYNC)
    {
        // ASYNC mode: Block until handler thread has processed a new update
        uint64_t current_updates;
        while ((current_updates = pctx->updates_processed.load(std::memory_order_acquire)) == pctx->updates_fetched)
        {
            // Create background flushes to invalidate signal storage cache
            // This emulates worst case for PCM when ACKs from datapath keep arriving
            // pctx->vm->flush_slab_input();
            // Keep spinning until new update from worker thread is available
        }
        if (pctx->updates_fetched + 1 != current_updates)
        {
            std::cout << "[WARNING] datapath and worker are out of sync: current_updates=" << current_updates << " updates_fetched=" << pctx->updates_fetched << " Skipping measurement." << std::endl;
            skip = true;
        }
        pctx->updates_fetched = current_updates;
    }
    // SYNC mode: No waiting needed, invoke already happened

    // Fetch latest control values from VM
    pctx->vm->fetch_slab_output();
    CcAlgorithmAdapter::ControlOutput out;
    out.cwnd = pctx->io_slab->out.cwnd;
    out.rate_bytes_per_s = pctx->io_slab->out.rate;
    out.ev = pctx->io_slab->out.ev;
    out.skip = skip;
    return out;
}

// Create PCM VM adapter
// algo_name: algorithm shared library name (e.g., "uec_dctcp")
// mode: PcmVmMode::SYNC or PcmVmMode::ASYNC
CcAlgorithmAdapter create_pcm_vm_adapter(const char *algo_name, PcmVmMode mode = PcmVmMode::ASYNC)
{
    // Load the algorithm's factory function
    std::string lib_name = std::string("lib") + algo_name + "_spec.so";
    std::string factory_fn;
    if (mode == PcmVmMode::ASYNC)
    {
        factory_fn = std::string("__") + algo_name + "_atomic_spec_get";
    }
    else
    {
        factory_fn = std::string("__") + algo_name + "_spec_get";
    }

    std::cout << "[PCM] Attempting to load library: " << lib_name << "\n";
    std::cout << "[PCM] Looking for factory function: " << factory_fn << "\n";

    auto result = pcm_vm::util::shared_symbol_open(lib_name, factory_fn);
    if (!result.has_value())
    {
        std::cerr << "[PCM] Failed to load PCM algorithm library: " << lib_name << "\n";
        std::cerr << "[PCM] Make sure the library is in LD_LIBRARY_PATH or same directory\n";
        throw std::runtime_error("Failed to load PCM algorithm: " + lib_name);
    }

    auto [so_handle, raw_fn_ptr] = result.value();
    auto factory = reinterpret_cast<pcm_vm::PcmHandlerVmDesc *(*)()>(raw_fn_ptr);

    std::cout << "[PCM] Successfully loaded library and factory function\n";
    std::cout << "[PCM] Mode: " << (mode == PcmVmMode::SYNC ? "sync" : "async") << "\n";

    // Create VM instance
    std::cout << "[PCM] Creating VM instance...\n";
    static pcm_vm::PcmHandlerVmDesc *vm = factory(); // Static to keep VM alive
    std::cout << "[PCM] VM instance created successfully\n";

    static pcm_vm::PcmHandlerVmDesc::PcmHandlerVmIoSlab *io_slab = vm->get_signal_io_slab();

    // Add time source (using steady_clock for microsecond precision)
    // vm->add_get_time_source([]() -> pcm_uint
    //                         { return (pcm_uint)std::chrono::duration_cast<std::chrono::microseconds>(
    //                                      std::chrono::steady_clock::now().time_since_epoch())
    //                               .count(); });
    // vm->add_get_time_source([]() -> pcm_uint
    //                         { return (pcm_uint)std::chrono::duration_cast<std::chrono::nanoseconds>(
    //                                      std::chrono::high_resolution_clock::now().time_since_epoch())
    //                               .count(); });
    // auto ghz = timing_lib::calibrate_tsc();
    vm->add_get_time_source([]() -> pcm_uint
                            { return (pcm_uint)(__rdtsc() / 2.3); });

    // Setup context
    static PcmVmContext pctx;
    pctx.vm = vm;
    pctx.io_slab = io_slab;
    pctx.mode = mode;
    pctx.running.store(true, std::memory_order_release);

    if (mode == PcmVmMode::ASYNC)
    {
        // Start handler thread (ASYNC mode only)
        pctx.handler_thread = std::thread(pcm_vm_handler_thread_fn, vm, &pctx.running, &pctx.updates_processed);
        std::cout << "[PCM] Handler thread started\n";
#ifdef __linux__
        // Pin handler thread to core 49 (sibling core) through native handle from std::thread
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(49, &cpuset); // optimal mapping for Intel(R) Xeon(R) Gold 6140 CPU @ 2.30GHz
        pthread_setaffinity_np(pctx.handler_thread.native_handle(), sizeof(cpu_set_t), &cpuset);
        std::cout << "[PCM] Handler thread pinned to core 1\n";
#endif
    }

#ifdef __linux__
    // Pin main thread to core 13
    cpu_set_t main_cpuset;
    CPU_ZERO(&main_cpuset);
    CPU_SET(13, &main_cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &main_cpuset);
    std::cout << "[PCM] Main thread pinned to core 0\n";
#endif

    CcAlgorithmAdapter adapter;
    adapter.context = &pctx;
    adapter.submit_event = pcm_vm_submit_event;
    adapter.invoke = pcm_vm_invoke;
    adapter.fetch_output = pcm_vm_fetch_output;

    std::cout << "[PCM] Adapter created successfully\n";
    return adapter;
}

// Cleanup PCM VM adapter - stop handler thread
void cleanup_pcm_vm_adapter(CcAlgorithmAdapter &adapter)
{
    auto *pctx = static_cast<PcmVmContext *>(adapter.context);

    if (pctx->mode == PcmVmMode::ASYNC)
    {
        // Signal handler thread to stop
        pctx->running.store(false, std::memory_order_release);

        // Wait for handler thread to exit
        if (pctx->handler_thread.joinable())
        {
            pctx->handler_thread.join();
            std::cout << "[PCM] Handler thread stopped\n";
        }
    }
}

// ---------- Main ----------
int main(int argc, char **argv)
{
    if (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help"))
    {
        std::cout << "Usage: " << argv[0] << " [iters] [seed] [no_loss] [mode] [algo] [pcm_mode]\n"
                  << "  iters: number of iterations (default: 100)\n"
                  << "  seed: random seed (default: 0xC0FFEE)\n"
                  << "  no_loss: 0 or 1 (default: 0)\n"
                  << "  mode: 'libccp' or 'pcm' (default: libccp)\n"
                  << "  algo: algorithm name (default: reno)\n"
                  << "  pcm_mode: 'sync' or 'async' (default: async, only for PCM mode)\n"
                  << "\nExamples:\n"
                  << "  libccp mode:     " << argv[0] << " 1000 42 0 libccp reno\n"
                  << "  PCM async mode:  " << argv[0] << " 1000 42 0 pcm dcqcn async\n"
                  << "  PCM sync mode:   " << argv[0] << " 1000 42 0 pcm dcqcn sync\n";
        return 0;
    }

    int iters = (argc > 1) ? std::max(1, std::atoi(argv[1])) : 100;
    uint64_t seed = (argc > 2) ? (uint64_t)std::stoull(argv[2]) : 0xC0FFEE;
    bool no_loss = (argc > 3) ? (std::atoi(argv[3]) != 0) : false;
    std::string mode = (argc > 4) ? argv[4] : "libccp";
    std::string algo = (argc > 5) ? argv[5] : "reno";
    std::string pcm_mode_str = (argc > 6) ? argv[6] : "async";

    // Parse PCM mode
    PcmVmMode pcm_mode = (pcm_mode_str == "sync") ? PcmVmMode::SYNC : PcmVmMode::ASYNC;

    // --- Generate random events upfront ---
    std::cout << "[main] Generating " << iters << " random network events (seed=" << seed << " no_loss=" << no_loss << ")...\n";
    std::vector<NetworkEvent> events = generate_event_trace(iters, seed, no_loss);

    CcAlgorithmAdapter cc_adapter;

    // Testbed instance that persists for entire execution
    LibccpTestbed tb;

    if (mode == "pcm")
    {
        // --- PCM VM mode ---
        std::cout << "[main] Using PCM VM mode with algorithm: " << algo << "\n";
        cc_adapter = create_pcm_vm_adapter(algo.c_str(), pcm_mode);
    }
    else
    {
        // --- libccp mode (default) ---
        std::cout << "[main] Using libccp mode with algorithm: " << algo << "\n";

        // Connect to Portus FIRST
        if (libccp_connect_to_portus() < 0)
        {
            return 1;
        }

        // Setup datapath
        if (libccp_setup_datapath(&tb) < 0)
        {
            close(portus_sock);
            return 2;
        }

        // Start one synthetic flow
        if (!libccp_create_flow(&tb, algo.c_str()))
        {
            ccp_free(&tb.dp);
            close(portus_sock);
            return 3;
        }

        // Create libccp adapter
        cc_adapter = create_libccp_adapter(&tb);
    }

    std::cout << "[main] Running " << iters << " events (seed=" << seed << ", mode=" << mode << ")\n";

    // --- Drive with pregenerated events ---
    for (int i = 0; i < iters; i++)
    {
        const auto &evt = events[i];

#if defined(PROFILE_FULL_CYCLE)
        timing.start();
#endif

        // 1) Submit event to datapath
        cc_adapter.submit_event(cc_adapter.context, evt);

        // 2) Invoke CC algorithm
        cc_adapter.invoke(cc_adapter.context);

        // 3) Fetch control output
        auto post_output = cc_adapter.fetch_output(cc_adapter.context);

#ifdef PROFILE_FULL_CYCLE
        timing.stop(post_output.skip ? false : true, "[submit->invoke->fetch]");
#endif

        // Clear one-shot flags (libccp-specific cleanup)
        if (mode == "libccp")
        {
            if (tb.active_conn)
            {
                tb.active_conn->prims.was_timeout = false;
            }
        }

        // Log post-invoke state
        // std::cout << "[iter " << i << ", outer_loop] cwnd=" << post_output.cwnd
        //           << " rate=" << post_output.rate_bytes_per_s << " ev=" << post_output.ev << " \n";
    }

    // --- Teardown ---
    std::cout << "[main] Cleaning up...\n";

    if (mode == "libccp")
    {
        // libccp cleanup
        if (tb.active_conn)
        {
            ccp_connection_free(&tb.dp, tb.active_conn->index);
            std::cout << "[conn] Freed sid=" << tb.active_conn->index << "\n";
        }

        // Close socket
        if (portus_sock >= 0)
        {
            close(portus_sock);
        }

        ccp_free(&tb.dp);
    }
    else if (mode == "pcm")
    {
        // PCM VM cleanup - stop handler thread
        cleanup_pcm_vm_adapter(cc_adapter);
    }

    std::cout << "[main] Exited cleanly.\n";
    return 0;
}
