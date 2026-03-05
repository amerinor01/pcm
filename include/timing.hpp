#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>

namespace timing_lib {

// Helper struct for stats
struct CalibrationStats {
    double mean{-1};
    uint64_t min{};
    uint64_t max{};
    uint64_t p95{};
    uint64_t p99{};
    double freq_ghz{};
};

class TimerPlaceholder {
  public:
    TimerPlaceholder() = default;
    TimerPlaceholder([[maybe_unused]] CalibrationStats stats) {}
    inline void start() {}
    inline std::pair<uint64_t, uint64_t>
    stop([[maybe_unused]] bool print,
         [[maybe_unused]] std::string_view report_prefix) {
        // string_veiw has no side effects, so this code gets eliminated
        return std::pair<uint64_t, uint64_t>{0, 0};
    }
    static std::string units() { return ""; }
};

#ifdef ENABLE_TIMING_LIB

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

// Backend 1: Standard Chrono (Nanoseconds)
class ChronoTimer {
  public:
    using Clock = std::chrono::high_resolution_clock;

    ChronoTimer() = default;
    ChronoTimer(CalibrationStats stats) : stats_{stats} {
        if (stats_.min <= 0) {
            std::cerr << "frequency and stats must be positive" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    inline void start() { start_time_ = Clock::now(); }

    // Returns elapsed time in nanoseconds
    inline std::pair<uint64_t, uint64_t> stop(bool print,
                                              std::string_view report_prefix) {
        auto end_time = Clock::now();
        auto tot_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            end_time - start_time_)
                            .count();
        tot_time = stats_.min > 0 ? tot_time - stats_.min : tot_time;
        last_measurement_ = std::pair<uint64_t, uint64_t>{tot_time, tot_time};
        if (print) {
            std::cout << report_prefix << ": " << *this << std::endl;
        }
        return last_measurement_;
    }

    static std::string units() { return "nanoseconds"; }

  private:
    const std::pair<uint64_t, uint64_t> &last() const {
        return last_measurement_;
    }
    friend std::ostream &operator<<(std::ostream &os, const ChronoTimer &t) {
        os << std::get<0>(t.last()) << " ns";
        return os;
    }

    CalibrationStats stats_{};
    std::chrono::time_point<Clock> start_time_;
    std::pair<uint64_t, uint64_t> last_measurement_;
};

// Backend 2: RDTSC with Serialization Barriers (Cycles)
// Methodology:
// Start: lfence -> rdtsc
// <measured code>
// Stop:  rdtscp -> lfence
class RdtscTimer {
  public:
    RdtscTimer() = default;
    RdtscTimer(CalibrationStats stats) : stats_{stats} {
        if (stats_.freq_ghz <= 0 or stats_.min <= 0) {
            std::cerr << "frequency and stats must be positive" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    inline void start() {
        // Prevent Out-of-Order execution of the measured code 'upwards'
        // past the timer start.
        _mm_lfence();
        start_tick_ = __rdtsc();
    }

    // Returns elapsed time in CPU Cycles and nanoseconds
    inline std::pair<uint64_t, uint64_t> stop(bool print,
                                              std::string_view report_prefix) {
        unsigned int aux;
        // Wait for all previous instructions to finish before reading timer
        uint64_t end_tick = __rdtscp(&aux);
        // Prevent future instructions from moving 'upwards' into the measured
        // zone
        _mm_lfence();
        auto tot_ticks = stats_.min > 0
                             ? ((end_tick - start_tick_) - stats_.min)
                             : (end_tick - start_tick_);
        last_measurement_ = std::pair<uint64_t, uint64_t>{
            tot_ticks, stats_.freq_ghz > 0
                           ? static_cast<uint64_t>(tot_ticks / stats_.freq_ghz)
                           : 0};
        if (print) {
            std::cout << report_prefix << ": " << *this << std::endl;
        }
        return last_measurement_;
    }

    static std::string units() { return "cycles"; }

  private:
    const std::pair<uint64_t, uint64_t> &last() const {
        return last_measurement_;
    }
    friend std::ostream &operator<<(std::ostream &os, const RdtscTimer &t) {
        os << std::get<0>(t.last()) << " cyc " << std::get<1>(t.last())
           << " ns";
        return os;
    }

    CalibrationStats stats_{};
    uint64_t start_tick_;
    std::pair<uint64_t, uint64_t> last_measurement_;
};

#ifdef __linux__
#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                            int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

class PerfInstrTimer {
  public:
    PerfInstrTimer() = default;
    PerfInstrTimer(CalibrationStats stats) : stats_{stats} {
        if (stats_.min <= 0) {
            std::cerr << "frequency and stats must be positive" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    ~PerfInstrTimer() {
        if (is_started_) {
            stop(false, "");
        }
    }

    inline void start() {
        if (is_started_) {
            std::cerr << "Attempt to start timer that was already started"
                      << std::endl;
            exit(EXIT_FAILURE);
        } else {
            is_started_ = true;
            struct perf_event_attr pe;
            memset(&pe, 0, sizeof(pe));
            pe.type = PERF_TYPE_HARDWARE;
            pe.size = sizeof(struct perf_event_attr);
            pe.config = PERF_COUNT_HW_INSTRUCTIONS;
            pe.disabled = 1;
            pe.exclude_kernel = 1;
            pe.exclude_hv = 1;

            fd_ = perf_event_open(&pe, 0, -1, -1, 0);
            if (fd_ == -1) {
                std::cerr << "pcm-perf: prof perf_event_open (instr) failed"
                          << std::endl;
                exit(EXIT_FAILURE);
            }
            if (ioctl(fd_, PERF_EVENT_IOC_RESET, 0)) {
                std::cerr << "pcm-perf: prof ioctl RESET failed" << std::endl;
                exit(EXIT_FAILURE);
            }
            if (ioctl(fd_, PERF_EVENT_IOC_ENABLE, 0)) {
                std::cerr << "pcm-perf: prof ioctl ENABLE failed" << std::endl;
                exit(EXIT_FAILURE);
            }
        }
    }

    // Returns elapsed time in Instructions
    inline std::pair<uint64_t, uint64_t> stop(bool print,
                                              std::string_view report_prefix) {
        // try stop timing first (even if it was never enabled)
        // if it was, then we'll get the precise measurement
        // if not, we'll fail in one of syscalls or later is is_started_ check

        if (ioctl(fd_, PERF_EVENT_IOC_DISABLE, 0)) {
            std::cerr << "pcm-perf: ioctl DISABLE failed" << std::endl;
            exit(EXIT_FAILURE);
        }

        uint64_t tot_instr = 0;
        if (read(fd_, &tot_instr, sizeof(tot_instr)) != sizeof(tot_instr)) {
            std::cerr << "pcm-perf: read failed" << std::endl;
            exit(EXIT_FAILURE);
        }

        if (close(fd_)) {
            std::cerr << "pcm-perf: close failed" << std::endl;
            exit(EXIT_FAILURE);
        }

        if (!is_started_) {
            std::cerr << "Attempt to stop timer that wasn't started"
                      << std::endl;
            exit(EXIT_FAILURE);
        } else {
            is_started_ = false;
        }

        tot_instr = stats_.min > 0 ? tot_instr - stats_.min : tot_instr;
        last_measurement_ = std::pair<uint64_t, uint64_t>{tot_instr, tot_instr};
        if (print) {
            std::cout << report_prefix << ": " << *this << std::endl;
        }
        return last_measurement_;
    }

    static std::string units() { return "instructions"; }

  private:
    const std::pair<uint64_t, uint64_t> &last() const {
        return last_measurement_;
    }

    friend std::ostream &operator<<(std::ostream &os, const PerfInstrTimer &t) {
        os << std::get<0>(t.last()) << " instr";
        return os;
    }

    bool is_started_{false};
    int fd_{-1};
    CalibrationStats stats_{};
    std::pair<uint64_t, uint64_t> last_measurement_;
};
#endif

#else

using RdtscTimer = TimerPlaceholder;
using ChronoTimer = TimerPlaceholder;
using PerfInstrTimer = TimerPlaceholder;

#endif

// Helper to determine TSC frequency (Cycles per Nanosecond)
inline double calibrate_tsc() {
    std::cout << "Calibrating TSC frequency...\n";
    ChronoTimer chrono;
    RdtscTimer rdtsc;

    chrono.start();
    rdtsc.start();

    // Sleep for 100ms to get a good sample
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    uint64_t cycles = std::get<0>(rdtsc.stop(false, ""));
    uint64_t ns = std::get<0>(chrono.stop(false, ""));

    double freq_ghz = (double)cycles / ns;
    std::cout << "Detected Frequency: " << freq_ghz << " cycles/ns ("
              << freq_ghz * 1000.0 << " MHz)\n";
    return freq_ghz;
}

// Configuration
const int ITERATIONS = 10000;
const int WARMUP_ITERATIONS = 100;

// Generic function to benchmark any timer backend
template <typename TimerType>
CalibrationStats benchmark_timer(const std::string &name) {
    if constexpr (std::is_same_v<TimerType, TimerPlaceholder>) {
        return CalibrationStats{};
    }

    std::vector<uint64_t> samples;
    samples.reserve(ITERATIONS);

    TimerType timer;

    std::cout << "--------------------------------------------------\n";
    std::cout << "Benchmarking backend: " << name << " (" << TimerType::units()
              << ")\n";
    std::cout << "Warmup:  " << WARMUP_ITERATIONS << " iters\n";
    std::cout << "Samples: " << ITERATIONS << " iters\n";

    // --- WARMUP PHASE ---
    // We run the timer loop but do not store the results.
    // This allows caches to warm up and the CPU branch predictor to learn the
    // loop.
    for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
        timer.start();
        volatile uint64_t dummy = std::get<0>(timer.stop(false, ""));
        (void)dummy; // Prevent compiler from optimizing away
    }

    // --- MEASUREMENT PHASE ---
    for (int i = 0; i < ITERATIONS; ++i) {
        timer.start();
        // We are measuring the cost of the timer itself (overhead)
        uint64_t elapsed = std::get<0>(timer.stop(false, ""));
        samples.push_back(elapsed);
    }

    // Calculate Statistics
    std::sort(samples.begin(), samples.end());

    uint64_t sum = std::accumulate(samples.begin(), samples.end(), 0ULL);

    CalibrationStats s;
    s.mean = static_cast<double>(sum) / ITERATIONS;
    s.min = samples.front();
    s.max = samples.back();
    s.p95 = samples[static_cast<size_t>(ITERATIONS * 0.95)];
    s.p99 = samples[static_cast<size_t>(ITERATIONS * 0.99)];
    s.freq_ghz = calibrate_tsc();

    std::cout << "Mean:    " << std::fixed << std::setprecision(2) << s.mean
              << "\n";
    std::cout << "Min:     " << s.min << "\n";
    std::cout << "Max:     " << s.max << "\n";
    std::cout << "P95:     " << s.p95 << "\n";
    std::cout << "P99:     " << s.p99 << "\n";
    std::cout << "RDTSC freq: " << s.freq_ghz << "\n";
    return s;
}

} // namespace timing_lib