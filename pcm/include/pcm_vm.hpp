#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <chrono>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <dlfcn.h> // dlopen/dlsym

#include "pcm.h"
#include "pcmh.h"
#include "prof.h"

/*
 Style guide:
 - Types in PascalCase e.g., VariableDesc, ControlDesc, SignalDesc,
 PcmHandlerVmDesc, etc).
 - Functions and non-type identifiers in lower_snake_case.
 - Class/struct data members end with a trailing underscore (e.g., get_time_,
 start_ts_).
 - Compile-time constants and enum values use k-prefixed PascalCase (e.g.,
 kIndex, kInitialValue).
 - Kept trait names in snake_case to match the STL convention (e.g.,
 is_variable_v).
*/

namespace pcm_vm {

namespace util {

// Dependently-false idiom for static_assertions in "if constexpr" branching:
// assert_always_false_v(always_false_v<void>, "error") is template-dependant,
// so it will be evaluated only if the constexpr branch with this static assert
// is taken (during template instantiation), since constexpr's are also type
// dependant.
template <typename> inline constexpr bool assert_always_false_v = false;

[[nodiscard]] inline std::optional<std::pair<void *, void *>>
shared_symbol_open(const std::string &lib_name, const std::string &fn_name) {
    void *so_handle = dlopen(lib_name.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!so_handle) {
        std::cerr << "dlopen(" << lib_name << ") failed with " << dlerror()
                  << std::endl;
        return std::nullopt;
    }

    void *fn_ptr = dlsym(so_handle, fn_name.c_str());
    if (!fn_ptr) {
        dlclose(so_handle);
        std::cerr << "dlsym(" << fn_name << ") failed with " << dlerror()
                  << std::endl;
        return std::nullopt;
    }

    return std::make_pair(so_handle, fn_ptr);
}

inline void shared_symbol_close(void *so_handle) {
    if (so_handle && dlclose(so_handle)) {
        std::cerr << "dlclose() failed with " << dlerror() << std::endl;
    }
}

using GetTimeFn = pcm_uint (*)();
inline uint64_t get_time_diff(pcm_uint ts_start, pcm_uint ts_end) {
    return ts_end - ts_start;
}

} // namespace util

// ============================================================================
// PcmHandlerVmDesc defines a VM runtime API which faces datapath.
//
// Once datapath opens a shared object with algorithm specification, it can
// lookup a factory function for PcmHandlerVmDesc (each datapath flow/connection
// needs a new algorithm VM instance).
//
// It's up to the datapath to schedule VM progress through calls to
// invoke_cc_algorithm_on_trigger.
//
// PcmHandlerVmDesc hides details of a child PcmHandlerVm struct which
// implements actual VM logic. Datapath can't interface PcmHandlerVm directly
// because it is a template class and instantiated for a particular algorithm.
// Exact instantiation type is not visible to datapath, which needs to know only
// an algorithm name to open shared objects.
// ============================================================================
struct PcmHandlerVmDesc {
    virtual ~PcmHandlerVmDesc() = default;
    virtual void add_get_time_source(util::GetTimeFn get_time_source) = 0;
    virtual bool invoke_cc_algorithm_on_trigger() = 0;

    // ============================================================================
    // PcmHandlerVmIoSlab is used for fast communication between datapath and VM
    //
    // IO slab is managed by VM. VM provides to datapath reference to slab.
    //
    // Input path: upon datapath events (ACKs/NACKs/etc.) datapath can register
    // register them in slab inputs PcmHandlerVmDesc::flush_slab_output.in and
    // call PcmHandlerVmDesc::flush_slab_input.
    //
    // Output path: when datapath needs a result of an algorith, it calls
    // PcmHandlerVmDesc::flush_slab_output, and can read fresh results in
    // PcmHandlerVmDesc::flush_slab_output.out.
    // ============================================================================
    struct PcmHandlerVmIoSlab {
        struct Input {
            pcm_uint ack{};
            pcm_uint rto{};
            pcm_uint nack{};
            pcm_uint ecn{};
            pcm_uint rtt{};
            pcm_uint data_tx{};
            pcm_uint data_nacked{};
            pcm_uint in_flight{};
            pcm_uint ack_ev{};
            pcm_uint ecn_ev{};
            pcm_uint nack_ev{};
            pcm_uint tx_ready_pkts{};
            pcm_uint tx_backlog_bytes{};
        } in;
        struct Output {
            pcm_uint cwnd{};
            pcm_uint rate{};
            pcm_uint ev{};
        } out;
    } io_slab_;

    PcmHandlerVmIoSlab *get_signal_io_slab() { return &io_slab_; };
    virtual void flush_slab_input() = 0;
    virtual void fetch_slab_output() = 0;

#ifdef ENABLE_PROFILING
    // Evaluate virtual call overhead
    virtual pcm_uint vcall_overhead_test() = 0;
#endif
};

// ============================================================================
// Definition of handler virtual machine state objects: variables, controls and
// signals.
// ============================================================================

// ============================================================================
// Each algorithm has to provide a specification to instantiate
// PcmHandlerVm template. A specification is a tuple (variadic template) of
// template objects of VariableDesc, ControlDesc and SignalDesc. During
// instantiation PcmHandlerVm distinguishes these objects from each other using
// traits: is_variable_v, is_control_v, is_signal_v.
//
// Further, ControlDesc and SignalDesc describe objects that are shared between
// datapath and handler - therefore they need a custom storage datatype that
// supports load/store/arithmetic operations for a given concurrency model.
//
// For example, a non-thread safe SimplePcmHandlerVm::PcmHandlerVm stores
// Signals and Controls as pcm_uint's. It implements datapath
// snapshotting with a simple plain memcpy.
//
// Instantiation of ControlPolicy and SignalPolicy with atomic storage type
// would yield a thread-safe code.
// ============================================================================

// ============================================================================
// Control definitions: generic descriptor + trait
// ============================================================================

// generic descriptor
template <pcm_control_t ControlType, pcm_uint InitialValue, pcm_uint Index>
struct ControlDesc {
    static constexpr pcm_control_t kType = ControlType;
    static constexpr pcm_uint kInitialValue = InitialValue;
    static constexpr pcm_uint kIndex = Index;
};

// trait
template <typename> struct is_control : std::false_type {};
template <pcm_control_t C, pcm_uint V, pcm_uint I>
struct is_control<ControlDesc<C, V, I>> : std::true_type {};
template <typename T>
inline constexpr bool is_control_v = is_control<std::decay_t<T>>::value;

// ============================================================================
// Signal definitions: generic descriptor + trait
// ============================================================================

// generic descriptor
template <pcm_signal_t SigType, pcm_signal_accum_t Accum,
          pcm_uint TriggerThreshold, pcm_uint Mask>
struct SignalDesc {
    static_assert(std::has_single_bit(Mask),
                  "Signal mask must be exactly one bit.");
    static constexpr pcm_signal_t kSigType = SigType;
    static constexpr pcm_uint kMask = Mask;
    static constexpr pcm_uint kIndex = std::countr_zero(Mask);
    static constexpr pcm_uint kTriggerThreshold = TriggerThreshold;
    static constexpr bool kIsTrigger =
        ((TriggerThreshold > 0) && (TriggerThreshold != PCM_SIG_NO_TRIGGER));
    static constexpr bool kIsAccum = (Accum != PCM_SIG_ACCUM_UNSPEC);

    static pcm_uint update(pcm_uint start_ts, util::GetTimeFn get_time,
                           pcm_uint curr, pcm_uint update_value)
        requires(Accum != PCM_SIG_ACCUM_UNSPEC)
    {
        // Time signal needs special treatment - it supports only monothonic
        // increase
        if constexpr (SigType == PCM_SIG_ELAPSED_TIME) {
            static_assert(Accum == PCM_SIG_ACCUM_SUM,
                          "PCM_SIG_ELAPSED_TIME supports only "
                          "PCM_SIG_ACCUM_SUM accumulator");
            return util::get_time_diff(start_ts, get_time());
        }

        if constexpr (Accum == PCM_SIG_ACCUM_SUM) {
            return curr + update_value;
        } else if constexpr (Accum == PCM_SIG_ACCUM_LAST) {
            return update_value;
        } else if constexpr (Accum == PCM_SIG_ACCUM_MIN) {
            return std::min(curr, update_value);
        } else if constexpr (Accum == PCM_SIG_ACCUM_MAX) {
            return std::max(curr, update_value);
        } else {
            static_assert(util::assert_always_false_v<void>,
                          "Unsupported accumulator");
            return 0;
        }
    }

    static bool is_fired(pcm_uint start_ts, util::GetTimeFn get_time,
                         pcm_uint cur)
        requires(TriggerThreshold != PCM_SIG_NO_TRIGGER)
    {
        if constexpr (SigType == PCM_SIG_ELAPSED_TIME) {
            auto timer = cur;
            if (timer) {
                pcm_uint now = util::get_time_diff(start_ts, get_time());
                // we assume that now value is always larger than timer value
                if (now - timer >= kTriggerThreshold) {
                    return true;
                }
            }
            return false;
        } else if constexpr (SigType == PCM_SIG_DATA_TX) {
            auto burst_len = cur;
            return burst_len ? (burst_len - 1) >= kTriggerThreshold : false;
        } else {
            return cur >= kTriggerThreshold;
        }
    }
};

// trait
template <typename> struct is_signal : std::false_type {};
template <pcm_signal_t SigType, pcm_signal_accum_t Accum, pcm_uint Thresh,
          pcm_uint Mask>
struct is_signal<SignalDesc<SigType, Accum, Thresh, Mask>> : std::true_type {};
template <typename T>
inline constexpr bool is_signal_v = is_signal<std::decay_t<T>>::value;

// ============================================================================
// Variable definition: descriptor + trait
// ============================================================================

// descriptor
template <typename DType, const DType &InitialValue, pcm_uint Index>
struct VariableDesc {
    static_assert(sizeof(DType) == sizeof(pcm_uint),
                  "Duplicate/non-uniform indices among variables");
    static constexpr pcm_uint kIndex = Index;
    static constexpr DType kInitialValue = InitialValue;
};

// trait
template <typename> struct is_variable : std::false_type {};
template <typename DType, const DType &InitialValue, pcm_uint Index>
struct is_variable<VariableDesc<DType, InitialValue, Index>> : std::true_type {
};
template <typename T>
inline constexpr bool is_variable_v = is_variable<std::decay_t<T>>::value;

// ============================================================================
// Handler virtual machine (PcmHandlerVm):
// - implements virtual datapath API.
// - relies on CRTP idiom: its child class (PcmHandlerVmImplT) must implement
// actual storage management for signals and controls.
// - takes a variadic template (Objs) that contains a tuple of objects
// ============================================================================
template <typename PcmHandlerVmImplT, const char *AlgoName,
          typename SnapshotLayout, typename... Objs>
struct PcmHandlerVm;

// ============================================================================
// Snapshot layout descriptor - holds compile-time offset constants
// ============================================================================
template <size_t TriggerMaskOffset, size_t SignalSetMaskOffset,
          size_t SignalOffset, size_t ControlOffset,
          size_t VariableOffset, size_t SnapshotSize>
struct SnapshotMemoryLayout {
    static constexpr size_t kTriggerMaskOffset = TriggerMaskOffset;
    static constexpr size_t kSignalSetMaskOffset = SignalSetMaskOffset;
    static constexpr size_t kSignalOffset = SignalOffset;
    static constexpr size_t kControlOffset = ControlOffset;
    static constexpr size_t kVariableOffset = VariableOffset;
    static constexpr size_t kSnapshotSize = SnapshotSize;
};

// Helper to validate that objects of a given type in tuple have dense indices
template <typename Tuple>
concept HasDenseIndices =
    []<typename... Ts>(std::type_identity<std::tuple<Ts...>>) {
        constexpr std::size_t n = sizeof...(Ts);
        std::array<pcm_uint, n> a{static_cast<std::size_t>(Ts::kIndex)...};
        std::sort(a.begin(), a.end());
        for (pcm_uint i = 0; i < n; ++i)
            if (a[i] != i)
                return false;
        return true;
    }(std::type_identity<Tuple>{});

// Helper to validate that objects of a given type fit into the snapshot
template <typename Tuple, size_t MaxSize>
concept FitsSnapshot =
    []<typename... Ts>(std::type_identity<std::tuple<Ts...>>) {
        constexpr std::size_t n = sizeof...(Ts);
        return n <= MaxSize;
    }(std::type_identity<Tuple>{});

template <typename PcmHandlerVmImplT, const char *AlgoName,
          typename SnapshotLayout, typename... Objs>
struct PcmHandlerVm : PcmHandlerVmDesc {
    // filter tuples of signals/controls/variables using their type traits
    using VariablesTupleT = decltype(std::tuple_cat(
        std::conditional_t<is_variable_v<Objs>, std::tuple<Objs>,
                           std::tuple<>>{}...));
    using ControlsTupleT = decltype(std::tuple_cat(
        std::conditional_t<is_control_v<Objs>, std::tuple<Objs>,
                           std::tuple<>>{}...));
    using SignalsTupleT = decltype(std::tuple_cat(
        std::conditional_t<is_signal_v<Objs>, std::tuple<Objs>,
                           std::tuple<>>{}...));

    // validate tuple correctness
    static_assert(FitsSnapshot<SignalsTupleT, ALGO_CONF_MAX_NUM_SIGNALS>,
                  "Signal tuple size is larger than snapshot");
    static_assert(HasDenseIndices<VariablesTupleT>,
                  "Duplicate/non-uniform indices among variables");
    static_assert(HasDenseIndices<ControlsTupleT>,
                  "Duplicate/non-uniform indices among controls");
    static_assert(HasDenseIndices<SignalsTupleT>,
                  "Duplicate/non-uniform indices among signals");

    // Handler opened from shared object
    std::shared_ptr<void> algorithm_so_handle_;
    pcm_handler_main_cb handler_main_cb_{nullptr};
    // Snapshot storage exposed to handler
    std::array<pcm_uint, SnapshotLayout::kSnapshotSize> raw_snapshot_;
    // Hooks to datapath time source
    util::GetTimeFn get_time_{nullptr};
    pcm_uint start_ts_{};

    explicit PcmHandlerVm() {
        auto result = util::shared_symbol_open(
            "lib" + std::string(AlgoName) + ".so",
            std::string(__algorithm_entry_point_symbol));
        if (!result.has_value())
            throw std::runtime_error("Failed to open algorithm shared library");

        auto [so_handle, raw_fn_ptr] = result.value();
        algorithm_so_handle_ =
            std::shared_ptr<void>(so_handle, [](void *handle) {
                if (handle) {
                    util::shared_symbol_close(handle);
                }
            });
        handler_main_cb_ = reinterpret_cast<pcm_handler_main_cb>(raw_fn_ptr);
    }

    void add_get_time_source(util::GetTimeFn get_time_source) override {
        get_time_ = get_time_source;
        start_ts_ = get_time_source();
    }

    template <pcm_control_t Match>
    [[nodiscard]] pcm_uint get_control_first_match() {
        constexpr const auto kControlUnused =
            std::numeric_limits<pcm_uint>::max();
        pcm_uint result = kControlUnused;
        (([&](auto *self) -> bool {
            using T = Objs;
            if constexpr (is_control_v<T>) {
                if (T::kType == Match) {
                    auto const &slot =
                        static_cast<PcmHandlerVmImplT const *>(self)
                            ->template control_slot_impl<T::kIndex>();
                    result = slot;
                    return true;
                }
            }
            return false;
        }(this) || ...));
        return result;
    }

    template <pcm_signal_t Match> void update_signals(pcm_uint v) {
        (([&](auto *self) {
             using T = Objs;
             if constexpr (is_signal_v<T>) {
                 if constexpr (T::kIsAccum) {
                     if (T::kSigType == Match) {
                         auto &slot =
                             static_cast<PcmHandlerVmImplT *>(self)
                                 ->template signal_slot_impl<T::kIndex>();
                         slot = T::update(start_ts_, get_time_, slot, v);
                     }
                 }
             }
         }(this)),
         ...);
    }

    void flush_slab_input() override {
        // implicitly instantiate update_signals for all signals that algorithm
        // can have - for useless signals no code will be emitted.
        update_signals<PCM_SIG_ACK>(io_slab_.in.ack);
        update_signals<PCM_SIG_RTO>(io_slab_.in.rto);
        update_signals<PCM_SIG_NACK>(io_slab_.in.nack);
        update_signals<PCM_SIG_ECN>(io_slab_.in.ecn);
        update_signals<PCM_SIG_RTT>(io_slab_.in.rtt);
        update_signals<PCM_SIG_DATA_TX>(io_slab_.in.data_tx);
        update_signals<PCM_SIG_DATA_NACKED>(io_slab_.in.data_nacked);
        update_signals<PCM_SIG_IN_FLIGHT>(io_slab_.in.in_flight);
        update_signals<PCM_SIG_ACK_EV>(io_slab_.in.ack_ev);
        update_signals<PCM_SIG_ECN_EV>(io_slab_.in.ecn_ev);
        update_signals<PCM_SIG_NACK_EV>(io_slab_.in.nack_ev);
        update_signals<PCM_SIG_TX_READY_PKTS>(io_slab_.in.tx_ready_pkts);
        update_signals<PCM_SIG_TX_BACKLOG_BYTES>(io_slab_.in.tx_backlog_bytes);
        // cleanup for the next flush
        io_slab_.in = PcmHandlerVmIoSlab::Input();
    }

    void fetch_slab_output() override {
        io_slab_.out.cwnd = get_control_first_match<PCM_CTRL_CWND>();
        io_slab_.out.rate = get_control_first_match<PCM_CTRL_RATE>();
        io_slab_.out.ev = get_control_first_match<PCM_CTRL_EV>();
    }

    [[nodiscard]] constexpr pcm_uint collect_trigger_mask() {
        pcm_uint trigger_mask = 0;
        (([&](auto *self) {
             using T = Objs;
             if constexpr (is_signal_v<T>) {
                 if constexpr (T::kIsTrigger) {
                     auto const &val =
                         static_cast<PcmHandlerVmImplT const *>(self)
                             ->template signal_slot_impl<T::kIndex>();
                     if (T::is_fired(start_ts_, get_time_, val)) {
                         trigger_mask |= T::kMask;
                     }
                 }
             }
         }(this)),
         ...);
        return trigger_mask;
    }

    [[nodiscard]] bool invoke_cc_algorithm_on_trigger() override {
        PCM_PERF_PROF_REGION_SCOPE_INIT(trigger_cycle, "TRIGGER CYCLE");
        PCM_PERF_PROF_REGION_START(trigger_cycle);

        pcm_uint trigger_mask = collect_trigger_mask();
        if (!trigger_mask) {
            PCM_PERF_PROF_REGION_END(trigger_cycle, false);
            return false;
        }

        // Pure elapsed time signals.
        // TODO: check if this breaks timer logic
        update_signals<PCM_SIG_ELAPSED_TIME>(0);

        static_cast<PcmHandlerVmImplT *>(this)
            ->prepare_pre_trigger_snapshot_impl(trigger_mask);
        if (handler_main_cb_(reinterpret_cast<pcm_handler_datapath_snapshot>(
                raw_snapshot_.data())) != PCM_SUCCESS) [[unlikely]] {
            // TODO: only this VM should die, not whole stack.
            std::cerr << "Algorithm exited with error. Abort." << std::endl;
            exit(EXIT_FAILURE);
        }
        static_cast<PcmHandlerVmImplT *>(this)
            ->apply_post_trigger_snapshot_impl();

        PCM_PERF_PROF_REGION_END(trigger_cycle, true);
        return true;
    }

#ifdef ENABLE_PROFILING
    pcm_uint vcall_overhead_test() override {
        volatile pcm_uint test = 42;
        return test;
    }
#endif
};

// ============================================================================
// "Simple" handler virtual machine implementation
// Simple means that storage for controls and signals is just pcm_uint and is
// not thread-safe.
//
// We follow two-step pattern for template specialization: a primary template
// definition for SimplePcmHandlerVm followed by specialization.
//
// Primary template allows to write readable code:
// using MySpec = std::tuple<ControlDesc<...>, SignalDesc<...>>;
// SimplePcmHandlerVm<"dcqcn", MySpec> vm;
// ============================================================================
template <const char *AlgoName, typename SnapshotLayout, typename Tuple>
struct SimplePcmHandlerVm;
template <const char *AlgoName, typename SnapshotLayout, typename... Ds>
struct SimplePcmHandlerVm<AlgoName, SnapshotLayout, std::tuple<Ds...>>
    : PcmHandlerVm<
          SimplePcmHandlerVm<AlgoName, SnapshotLayout, std::tuple<Ds...>>,
          AlgoName, SnapshotLayout, Ds...> {

    using SelfType =
        SimplePcmHandlerVm<AlgoName, SnapshotLayout, std::tuple<Ds...>>;
    using Base = PcmHandlerVm<SelfType, AlgoName, SnapshotLayout, Ds...>;

    using ControlsTupleT = typename Base::ControlsTupleT;
    using SignalsTupleT = typename Base::SignalsTupleT;

    static constexpr std::size_t kNumControls =
        std::tuple_size<ControlsTupleT>::value;
    static constexpr std::size_t kNumSignals =
        std::tuple_size<SignalsTupleT>::value;

    // pcm_uint storage
    std::array<pcm_uint, kNumControls> controls_storage_{};
    std::array<pcm_uint, kNumSignals> signals_storage_{};

    explicit SimplePcmHandlerVm() : Base() {
        (([&] {
             using T = Ds;
             if constexpr (is_control_v<T>) {
                 controls_storage_[T::kIndex] = T::kInitialValue;
             } else if constexpr (is_variable_v<T>) {
                 // Snapshot variables can be initialized directly
                 Base::raw_snapshot_[SnapshotLayout::kVariableOffset +
                                     T::kIndex] =
                     std::bit_cast<pcm_uint>(T::kInitialValue);
             }
         }()),
         ...);
    }

    void prepare_pre_trigger_snapshot_impl(pcm_uint trigger_mask) {
        Base::raw_snapshot_[SnapshotLayout::kTriggerMaskOffset] = trigger_mask;
        Base::raw_snapshot_[SnapshotLayout::kSignalSetMaskOffset] = 0;
        std::memcpy(&Base::raw_snapshot_[SnapshotLayout::kSignalOffset],
                    signals_storage_.data(), kNumSignals * sizeof(pcm_uint));
        std::memset(signals_storage_.data(), 0, kNumSignals * sizeof(pcm_uint));
        std::memcpy(&Base::raw_snapshot_[SnapshotLayout::kControlOffset],
                    controls_storage_.data(), kNumControls * sizeof(pcm_uint));
    }

    void apply_post_trigger_snapshot_impl() {
        // update signals from snapshot
        auto set_signal_mask =
            Base::raw_snapshot_[SnapshotLayout::kSignalSetMaskOffset];
        (([&](auto *self) {
             using T = Ds;
             if constexpr (is_signal_v<T>) {
                 if (set_signal_mask & T::kMask) {
                     auto &slot = static_cast<SelfType *>(self)
                                      ->template signal_slot_impl<T::kIndex>();
                     slot = T::update(
                         Base::start_ts_, Base::get_time_, slot,
                         Base::raw_snapshot_[SnapshotLayout::kSignalOffset +
                                             T::kIndex]);
                 }
             }
         }(this)),
         ...);
        std::memcpy(controls_storage_.data(),
                    &Base::raw_snapshot_[SnapshotLayout::kControlOffset],
                    kNumControls * sizeof(pcm_uint));
    }

    template <std::size_t I> pcm_uint &control_slot_impl() {
        static_assert(I < kNumControls);
        return controls_storage_[I];
    }
    template <std::size_t I> const pcm_uint &control_slot_impl() const {
        static_assert(I < kNumControls);
        return controls_storage_[I];
    }
    template <std::size_t I> pcm_uint &signal_slot_impl() {
        static_assert(I < kNumSignals);
        return signals_storage_[I];
    }
    template <std::size_t I> const pcm_uint &signal_slot_impl() const {
        static_assert(I < kNumSignals);
        return signals_storage_[I];
    }
};

} // namespace pcm_vm