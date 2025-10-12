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
#include <vector>

#include <dlfcn.h> // dlopen/dlsym

#include "pcm.h"
#include "pcmh.h"

#include "../src/prof.h"

/*
 Style guide:
 - Types in PascalCase        auto [so_handle, raw_fn_ptr] = result.value();
        algorithm_so_library_handle_ = so_handle;
        algorithm_cb_ = reinterpret_cast<pcm_cc_algorithm_cb>(raw_fn_ptr);

        (([&] {g., VariableDesc, ControlDesc, SignalDesc, FlowDesc, SimpleFlow).
 - Functions and non-type identifiers in lower_snake_case.
 - Class/struct data members end with a trailing underscore (e.g., get_time_,
 start_ts_).
 - Compile-time constants and enum values use k-prefixed PascalCase (e.g.,
 kIndex, kInitialValue).
 - Kept trait names in snake_case to match the STL convention (e.g.,
 is_variable_v).
*/

namespace pcm {

namespace util {
template <typename> inline constexpr bool always_false_v = false;

[[nodiscard]] std::optional<std::pair<void *, void *>>
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

void shared_symbol_close(void *so_handle) {
    if (so_handle && dlclose(so_handle)) {
        std::cerr << "dlclose() failed with " << dlerror() << std::endl;
    }
}

using GetTimeFn = pcm_uint (*)();
uint64_t get_time_diff(pcm_uint ts_start, pcm_uint ts_end) {
    return ts_end - ts_start;
}

} // namespace util

// ============================================================================
// Forward declarations
// ============================================================================

// ============================================================================
// PCMI-local variable descriptor definitions: the simplest form of object
// associated with the flow
// ============================================================================

// User-exposed variable API
template <typename DType, const DType &InitialValue, pcm_uint Index>
struct VariableDesc {
    static_assert(sizeof(DType) == sizeof(pcm_uint),
                  "Duplicate/non-uniform indices among variables");
    static constexpr pcm_uint kIndex = Index;
    static constexpr DType kInitialValue = InitialValue;
    template <typename /*StorageT*/>
    using Rebind = VariableDesc<DType, InitialValue, Index>;
};

// Variable trait (kept in snake_case to mirror std traits style)
template <typename T> struct is_variable : std::false_type {};
template <typename D, const D &V, pcm_uint I>
struct is_variable<VariableDesc<D, V, I>> : std::true_type {};
template <typename T>
inline constexpr bool is_variable_v = is_variable<std::decay_t<T>>::value;

// ============================================================================
// Stateless control object policy
// ============================================================================
template <typename StorageT, pcm_control_t ControlType, pcm_uint InitialValue,
          pcm_uint Index>
struct ControlPolicy {
    static constexpr pcm_control_t kType = ControlType;
    static constexpr pcm_uint kInitialValue = InitialValue;
    static constexpr pcm_uint kIndex = Index;

    static void set(StorageT &slot, pcm_uint v) {
        slot = static_cast<StorageT>(v);
    }
    static pcm_uint get(const StorageT &slot) {
        return static_cast<pcm_uint>(slot);
    }
};

// user-facing control descriptor
template <pcm_control_t Type, pcm_uint InitialValue, pcm_uint Index>
struct ControlDesc {
    template <typename StorageT>
    using Rebind = ControlPolicy<StorageT, Type, InitialValue, Index>;
};

// Control trait
template <typename T> struct is_control : std::false_type {};
template <typename S, pcm_control_t C, pcm_uint V, pcm_uint I>
struct is_control<ControlPolicy<S, C, V, I>> : std::true_type {};
template <typename T>
inline constexpr bool is_control_v = is_control<std::decay_t<T>>::value;

// ============================================================================
// Stateless signal object policy
// ============================================================================

template <typename StorageT, pcm_signal_t Type, pcm_signal_accum_t Accum,
          pcm_uint TriggerThreshold, pcm_uint Mask>
struct SignalPolicy {
    static_assert(std::has_single_bit(Mask),
                  "Signal mask must be exactly one bit.");
    static constexpr pcm_signal_t kType = Type;
    static constexpr pcm_uint kMask = Mask;
    static constexpr pcm_uint kIndex = std::countr_zero(Mask);
    static constexpr pcm_uint kInitialTriggerThreshold = TriggerThreshold;
    static constexpr bool kIsTrigger =
        ((TriggerThreshold > 0) && (TriggerThreshold != PCM_SIG_NO_TRIGGER));
    static constexpr bool kIsAccum = (Accum != PCM_SIG_ACCUM_UNSPEC);

    static void update(pcm_uint start_ts, util::GetTimeFn get_time,
                       StorageT &cur, pcm_uint update_value)
        requires(Accum != PCM_SIG_ACCUM_UNSPEC)
    {
        const StorageT v = static_cast<StorageT>(update_value);
        if constexpr (Type == PCM_SIG_ELAPSED_TIME) {
            if constexpr (kIsTrigger) {
                // this is a timer handled in the TriggerSignal: nothing to do
            } else {
                cur = util::get_time_diff(start_ts, get_time());
            }
        } else if constexpr (Accum == PCM_SIG_ACCUM_SUM) {
            cur += v;
        } else if constexpr (Accum == PCM_SIG_ACCUM_LAST) {
            cur = v;
        } else if constexpr (Accum == PCM_SIG_ACCUM_MIN) {
            cur = std::min(cur, v);
        } else if constexpr (Accum == PCM_SIG_ACCUM_MAX) {
            cur = std::max(cur, v);
        } else {
            static_assert(util::always_false_v<void>,
                          "Unsupported accumulator");
        }
    }

    static bool is_fired(pcm_uint start_ts, util::GetTimeFn get_time,
                         const StorageT &cur, const StorageT &thresh)
        requires(TriggerThreshold != PCM_SIG_NO_TRIGGER)
    {
        if constexpr (Type == PCM_SIG_ELAPSED_TIME) {
            auto timer = cur;
            if (timer) {
                pcm_uint now = util::get_time_diff(start_ts, get_time());
                // we assume that now value is always larger than timer value
                if (now - timer >= thresh) {
                    return true;
                }
            }
            return false;
        } else if constexpr (Type == PCM_SIG_DATA_TX) {
            auto burst_len = cur;
            return burst_len ? (burst_len - 1) >= thresh : false;
        } else {
            return cur >= thresh;
        }
    }

    static void rearm_if_needed(pcm_uint start_ts, util::GetTimeFn get_time,
                                StorageT &cur)
        requires(TriggerThreshold != PCM_SIG_NO_TRIGGER)
    {
        if constexpr (kType == PCM_SIG_ELAPSED_TIME) {
            if (cur == PCM_SIG_REARM) {
                cur = util::get_time_diff(start_ts, get_time());
            }
        } else if constexpr (kType == PCM_SIG_DATA_TX) {
            if (cur == PCM_SIG_REARM) {
                cur = static_cast<StorageT>(1);
            }
        }
    }
};

// user-facing signal descriptor
template <pcm_signal_t Type, pcm_signal_accum_t Accum,
          pcm_uint TriggerThreshold, pcm_uint Mask>
struct SignalDesc {
    template <typename StorageT>
    using Rebind = SignalPolicy<StorageT, Type, Accum, TriggerThreshold, Mask>;
};

// Signal trait
template <typename T> struct is_signal : std::false_type {};
template <typename S, pcm_signal_t T, pcm_signal_accum_t Accum, pcm_uint Thr,
          pcm_uint M>
struct is_signal<SignalPolicy<S, T, Accum, Thr, M>> : std::true_type {};
template <typename T>
inline constexpr bool is_signal_v = is_signal<std::decay_t<T>>::value;

// ============================================================================
// Flow (type-iterating; no runtime control/signal objects)
// ============================================================================

// ============================================================================
// Flow descriptor (type-erased base)
// ============================================================================
struct FlowDesc {
    virtual ~FlowDesc() = default;
    virtual void add_get_time_source(util::GetTimeFn get_time_source) = 0;
    virtual bool invoke_cc_algorithm_on_trigger() = 0;
    virtual void update_controls_runtime(pcm_control_t ctrl,
                                         pcm_uint value) = 0;
    virtual pcm_uint get_control_first_match_runtime(pcm_control_t ctrl) = 0;
    virtual void update_signals_runtime(pcm_signal_t sig, pcm_uint value) = 0;
};

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

template <typename FlowImplT, typename... Objs> struct Flow : FlowDesc {
    using VariablesTupleT = decltype(std::tuple_cat(
        std::conditional_t<is_variable_v<Objs>, std::tuple<Objs>,
                           std::tuple<>>{}...));
    using ControlsTupleT = decltype(std::tuple_cat(
        std::conditional_t<is_control_v<Objs>, std::tuple<Objs>,
                           std::tuple<>>{}...));
    using SignalsTupleT = decltype(std::tuple_cat(
        std::conditional_t<is_signal_v<Objs>, std::tuple<Objs>,
                           std::tuple<>>{}...));

    static_assert(HasDenseIndices<VariablesTupleT>,
                  "Duplicate/non-uniform indices among variables");
    static_assert(HasDenseIndices<ControlsTupleT>,
                  "Duplicate/non-uniform indices among controls");
    static_assert(HasDenseIndices<SignalsTupleT>,
                  "Duplicate/non-uniform indices among signals");

    util::GetTimeFn get_time_{nullptr};
    pcm_uint start_ts_{};

    void add_get_time_source(util::GetTimeFn get_time_source) override {
        get_time_ = get_time_source;
        start_ts_ = get_time_source();
    }

    /*
     * set/get_control() can be called by the datapath, therefore has no
     * compile-time checks if match was found or not, the API intention is to
     * communicate is_found bool as return type so that callee can handle
     * (possibly) erroneous not found case
     */
    template <pcm_control_t Match>
    [[nodiscard]] bool set_first_match_control(pcm_uint val) {
        return (([&](auto *self) -> bool {
            using T = Objs;
            if constexpr (is_control_v<T>) {
                if (T::kType == Match) {
                    auto &slot = static_cast<FlowImplT *>(self)
                                     ->template control_slot_impl<T::kIndex>();
                    T::set(slot, val);
                    return true;
                }
            }
            return false;
        }(this) || ...));
    }

    template <pcm_control_t Match>
    [[nodiscard]] std::optional<pcm_uint> get_control_first_match() {
        std::optional<pcm_uint> result = std::nullopt;
        (([&](auto *self) -> bool {
            using T = Objs;
            if constexpr (is_control_v<T>) {
                if (T::kType == Match) {
                    auto const &slot =
                        static_cast<FlowImplT const *>(self)
                            ->template control_slot_impl<T::kIndex>();
                    result = T::get(slot);
                    return true;
                }
            }
            return false;
        }(this) || ...));
        return result;
    }

    pcm_uint get_control_first_match_runtime(pcm_control_t ctrl) override {
        std::optional<pcm_uint> out = std::nullopt;
        switch (ctrl) {
        case PCM_CTRL_CWND:
            out = get_control_first_match<PCM_CTRL_CWND>();
            break;
        case PCM_CTRL_RATE:
            out = get_control_first_match<PCM_CTRL_RATE>();
            break;
        case PCM_CTRL_EV:
            out = get_control_first_match<PCM_CTRL_EV>();
            break;
        default:
            throw std::runtime_error("unsupported control");
        }
        if (!out.has_value())
            throw std::runtime_error("control not found");
        return out.value();
    }

    void update_controls_runtime(pcm_control_t ctrl, pcm_uint value) override {
        bool found = false;
        switch (ctrl) {
        case PCM_CTRL_CWND:
            found = set_first_match_control<PCM_CTRL_CWND>(value);
            break;
        case PCM_CTRL_RATE:
            found = set_first_match_control<PCM_CTRL_RATE>(value);
            break;
        case PCM_CTRL_EV:
            found = set_first_match_control<PCM_CTRL_EV>(value);
            break;
        default:
            throw std::runtime_error("unsupported control");
        }
        if (!found) {
            throw std::runtime_error("control not found");
        }
    }

    void update_signals_runtime(pcm_signal_t sig, pcm_uint value) override {
        switch (sig) {
        case PCM_SIG_ACK:
            update_signals<PCM_SIG_ACK>(value);
            break;
        case PCM_SIG_RTO:
            update_signals<PCM_SIG_RTO>(value);
            break;
        case PCM_SIG_NACK:
            update_signals<PCM_SIG_NACK>(value);
            break;
        case PCM_SIG_ECN:
            update_signals<PCM_SIG_ECN>(value);
            break;
        case PCM_SIG_RTT:
            update_signals<PCM_SIG_RTT>(value);
            break;
        case PCM_SIG_DATA_TX:
            update_signals<PCM_SIG_DATA_TX>(value);
            break;
        case PCM_SIG_DATA_NACKED:
            update_signals<PCM_SIG_DATA_NACKED>(value);
            break;
        case PCM_SIG_IN_FLIGHT:
            update_signals<PCM_SIG_IN_FLIGHT>(value);
            break;
        case PCM_SIG_ELAPSED_TIME:
            update_signals<PCM_SIG_ELAPSED_TIME>(value);
            break;
        case PCM_SIG_ACK_EV:
            update_signals<PCM_SIG_ACK_EV>(value);
            break;
        case PCM_SIG_ECN_EV:
            update_signals<PCM_SIG_ECN_EV>(value);
            break;
        case PCM_SIG_NACK_EV:
            update_signals<PCM_SIG_NACK_EV>(value);
            break;
        case PCM_SIG_TX_BACKLOG_PKTS:
            update_signals<PCM_SIG_TX_BACKLOG_PKTS>(value);
            break;
        default:
            throw std::runtime_error("unsupported signal");
        }
    }

    template <pcm_signal_t Match> void update_signals(pcm_uint v) {
        (([&](auto *self) {
             using T = Objs;
             if constexpr (is_signal_v<T>) {
                 if constexpr (T::kIsAccum) {
                     if (T::kType == Match) {
                         auto &slot =
                             static_cast<FlowImplT *>(self)
                                 ->template signal_slot_impl<T::kIndex>();
                         T::update(start_ts_, get_time_, slot, v);
                     }
                 }
             }
         }(this)),
         ...);
    }

    [[nodiscard]] bool invoke_cc_algorithm_on_trigger() override {
        PCM_PERF_PROF_REGION_SCOPE_INIT(trigger_cycle, "TRIGGER CYCLE");
        PCM_PERF_PROF_REGION_START(trigger_cycle);

        pcm_uint trigger_mask = 0;

        // collect triggers into the mask
        (([&](auto *self) {
             using T = Objs;
             if constexpr (is_signal_v<T>) {
                 if constexpr (T::kIsTrigger) {
                     auto const &val =
                         static_cast<FlowImplT const *>(self)
                             ->template signal_slot_impl<T::kIndex>();
                     auto const &thresh =
                         static_cast<FlowImplT const *>(self)
                             ->template thresh_slot_impl<T::kIndex>();
                     if (T::is_fired(start_ts_, get_time_, val, thresh)) {
                         trigger_mask |= T::kMask;
                     }
                 }
             }
         }(this)),
         ...);

        if (!trigger_mask) {
            PCM_PERF_PROF_REGION_END(trigger_cycle, trigger_mask);
            return false;
        }

        update_signals<PCM_SIG_ELAPSED_TIME>(0);
        static_cast<FlowImplT *>(this)->prepare_pre_trigger_snapshot_impl(
            trigger_mask);
        (void)static_cast<FlowImplT *>(this)->execute_algorithm_impl();
        static_cast<FlowImplT *>(this)->apply_post_trigger_snapshot_impl();

        // rearm triggers
        (([&](auto *self) {
             using T = Objs;
             if constexpr (is_signal_v<T>) {
                 if constexpr (T::kIsTrigger) {
                     auto &val = static_cast<FlowImplT *>(self)
                                     ->template signal_slot_impl<T::kIndex>();
                     T::rearm_if_needed(start_ts_, get_time_, val);
                 }
             }
         }(this)),
         ...);
        PCM_PERF_PROF_REGION_END(trigger_cycle, trigger_mask);
        return true;
    }
};

// ============================================================================
// Simple flow implementation
// ============================================================================

// Primary template
// AlgoName is raw pointer because templates can't accept std::string
template <const char *AlgoName, typename Tuple> struct SimpleFlow;

template <const char *AlgoName, typename... Ds>
struct SimpleFlow<AlgoName, std::tuple<Ds...>>
    : Flow<SimpleFlow<AlgoName, std::tuple<Ds...>>,
           typename Ds::template Rebind<pcm_uint>...> {

    using SelfType = SimpleFlow<AlgoName, std::tuple<Ds...>>;
    using Base = Flow<SelfType, typename Ds::template Rebind<pcm_uint>...>;

    std::shared_ptr<void> algorithm_so_handle_;
    pcm_cc_algorithm_cb algorithm_cb_{nullptr};

    flow_datapath_snapshot snapshot_{};

    using ControlsTupleT = typename Base::ControlsTupleT;
    using SignalsTupleT = typename Base::SignalsTupleT;

    static constexpr std::size_t kNumControls =
        std::tuple_size<ControlsTupleT>::value;
    static constexpr std::size_t kNumSignals =
        std::tuple_size<SignalsTupleT>::value;

    std::array<pcm_uint, kNumControls> controls_storage_{};
    std::array<pcm_uint, kNumSignals> signals_storage_{};
    std::array<pcm_uint, kNumSignals> thresholds_storage_{};

    explicit SimpleFlow() {
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
        algorithm_cb_ = reinterpret_cast<pcm_cc_algorithm_cb>(raw_fn_ptr);

        (([&] {
             using T = typename Ds::template Rebind<pcm_uint>;
             if constexpr (is_signal_v<T>) {
                 if constexpr (T::kIsTrigger) {
                     thresholds_storage_[T::kIndex] =
                         T::kInitialTriggerThreshold;
                 }
             } else if constexpr (is_control_v<T>) {
                 controls_storage_[T::kIndex] = T::kInitialValue;
             } else if constexpr (is_variable_v<T>) {
                 // Snapshot variables can be initialized directly
                 snapshot_.vars[T::kIndex] =
                     std::bit_cast<pcm_uint>(T::kInitialValue);
             }
         }()),
         ...);
    }

    [[nodiscard]] pcm_err_t execute_algorithm_impl() {
        if (!algorithm_cb_)
            throw std::runtime_error("CC Algorithm callback is nullptr");
        return algorithm_cb_(&snapshot_);
    }

    void prepare_pre_trigger_snapshot_impl(pcm_uint trigger_mask) {
        snapshot_.trigger_mask = trigger_mask;
        std::memcpy(snapshot_.signals, signals_storage_.data(),
                    kNumSignals * sizeof(pcm_uint));
        std::memset(signals_storage_.data(), 0, kNumSignals * sizeof(pcm_uint));
        std::memcpy(snapshot_.thresholds, thresholds_storage_.data(),
                    kNumSignals * sizeof(pcm_uint));
        std::memcpy(snapshot_.controls, controls_storage_.data(),
                    kNumControls * sizeof(pcm_uint));
    }

    void apply_post_trigger_snapshot_impl() {
        // update signals from snapshot
        [this]<std::size_t... Is>(std::index_sequence<Is...>) {
            (([this]<std::size_t sig_idx>() {
                 auto &slot = signal_slot_impl<sig_idx>();
                 using SignalType =
                     std::tuple_element_t<sig_idx, SignalsTupleT>;
                 SignalType::update(Base::start_ts_, Base::get_time_, slot,
                                    snapshot_.signals[sig_idx]);
             }.template operator()<Is>()),
             ...);
        }(std::make_index_sequence<kNumSignals>{});
        std::memcpy(thresholds_storage_.data(), snapshot_.thresholds,
                    kNumSignals * sizeof(pcm_uint));
        std::memcpy(controls_storage_.data(), snapshot_.controls,
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
    template <std::size_t I> pcm_uint &thresh_slot_impl() {
        static_assert(I < kNumSignals);
        return thresholds_storage_[I];
    }
    template <std::size_t I> const pcm_uint &thresh_slot_impl() const {
        static_assert(I < kNumSignals);
        return thresholds_storage_[I];
    }
};

struct Device {
    util::GetTimeFn get_time_source_{nullptr};
    explicit Device(util::GetTimeFn get_time_source)
        : get_time_source_{get_time_source} {}

    [[nodiscard]] virtual std::optional<uint32_t> progress() = 0;
    [[nodiscard]] virtual bool progress(uint32_t id) = 0;

    void add_flow_spec_factory(std::function<FlowDesc *()> spec_factory,
                               uint32_t matching_rule) {
        if (!get_time_source_)
            throw std::runtime_error("Time source is nullptr");
        configs_.emplace_back(
            matching_rule, [this, spec_factory]() -> std::unique_ptr<FlowDesc> {
                std::unique_ptr<FlowDesc> new_flow(spec_factory());
                new_flow->add_get_time_source(get_time_source_);
                return new_flow;
            });
    }

    FlowDesc &create_flow(uint32_t new_id) {
        // id has to be unique
        if (flows_.find(new_id) != flows_.end())
            throw std::runtime_error("Flow with this id already exists");

        if (configs_.size() > 1)
            throw std::runtime_error("Device supports only a single CC config");

        // find spec and create
        for (const auto &[mask, factory] : configs_) {
            // for now we don't really support matching
            // if (new_id & mask) {
            if (true) {
                auto ret = flows_.insert({new_id, factory()});
                if (!ret.second)
                    throw std::runtime_error("new flow insertion failed");
                return *(ret.first->second);
            }
        }

        throw std::runtime_error("No matching flow spec for id");
    }

    void destroy_flow(uint32_t id) {
        auto it = flows_.find(id);
        if (it == flows_.end())
            throw std::runtime_error("Flow with this id doesn't exist");
        flows_.erase(it);
    }

  protected:
    using Factory = std::function<std::unique_ptr<FlowDesc>()>;
    std::vector<std::pair<uint32_t, Factory>> configs_; // mask + flow factory
    using FlowStorage = std::unordered_map<uint32_t, std::unique_ptr<FlowDesc>>;
    FlowStorage flows_{}; // flows
};

struct DeviceCheckOnSched final : Device {
    explicit DeviceCheckOnSched(util::GetTimeFn get_time_source)
        : Device::Device{get_time_source} {}

    std::optional<uint32_t> progress() override {
        if (flows_.empty())
            return std::nullopt;

        std::optional<uint32_t> id = std::nullopt;
        auto triggered = cur_rr_it_->second->invoke_cc_algorithm_on_trigger();
        if (triggered)
            id = cur_rr_it_->first;

        ++cur_rr_it_;
        if (cur_rr_it_ == flows_.end())
            cur_rr_it_ = flows_.begin();
        return id;
    }

    bool progress(uint32_t id) override {
        if (flows_.empty())
            throw std::runtime_error("Device flow storage is empty");

        auto it = flows_.find(id);
        if (it == flows_.end())
            throw std::runtime_error("id doesn't exist in flow storage");

        auto triggered = it->second->invoke_cc_algorithm_on_trigger();
        if (triggered)
            return true;

        return false;
    }

    FlowDesc &create_flow(uint32_t new_id) {
        auto &new_flow = Device::create_flow(new_id);
        cur_rr_it_ = flows_.begin();
        return new_flow;
    }

  private:
    Device::FlowStorage::iterator cur_rr_it_{Device::flows_.end()};
};

} // namespace pcm