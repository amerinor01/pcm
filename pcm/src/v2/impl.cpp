#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <chrono>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <vector>

#include <dlfcn.h> // dlopen/dlsym

#include "pcm.h"
#include "pcmh.h"

/*
 Style guide:
 - Types in PascalCase (e.g., VariableDesc, ControlDesc, SignalDesc, FlowDesc, SimpleFlow).
 - Functions and non-type identifiers in lower_snake_case.
 - Class/struct data members end with a trailing underscore (e.g., get_time_, start_ts_).
 - Compile-time constants and enum values use k-prefixed PascalCase (e.g., kIndex, kInitialValue,
 GetTimeBackend::kStd).
 - Kept trait names in snake_case to match the STL convention (e.g., is_variable_v).
*/

namespace pcm {

// ============================================================================
// Forward declarations
// ============================================================================
template <typename> inline constexpr bool always_false_v = false;

// ============================================================================
// PCMI-local variable descriptor definitions: the simplest form of object associated with the flow
// ============================================================================

// User-exposed variable API
template <typename DType, const DType &InitialValue, pcm_uint Index> struct VariableDesc {
    static_assert(sizeof(DType) == sizeof(pcm_uint),
                  "Duplicate/non-uniform indices among variables");
    static constexpr pcm_uint kIndex = Index;
    static constexpr DType kInitialValue = InitialValue;
    template <typename /*StorageT*/> using Rebind = VariableDesc<DType, InitialValue, Index>;
};

// Variable trait (kept in snake_case to mirror std traits style)
template <typename T> struct is_variable : std::false_type {};
template <typename D, const D &V, pcm_uint I>
struct is_variable<VariableDesc<D, V, I>> : std::true_type {};
template <typename T> inline constexpr bool is_variable_v = is_variable<std::decay_t<T>>::value;

// ============================================================================
// Stateless control object policy
// ============================================================================
template <typename StorageT, pcm_control_t ControlType, pcm_uint InitialValue, pcm_uint Index>
struct ControlPolicy {
    static constexpr pcm_control_t kType = ControlType;
    static constexpr pcm_uint kInitialValue = InitialValue;
    static constexpr pcm_uint kIndex = Index;

    static void set(StorageT &slot, pcm_uint v) { slot = static_cast<StorageT>(v); }
    static pcm_uint get(const StorageT &slot) { return static_cast<pcm_uint>(slot); }
};

// user-facing control descriptor -> rebind to policy implementation
template <pcm_control_t Type, pcm_uint InitialValue, pcm_uint Index> struct ControlDesc {
    template <typename StorageT> using Rebind = ControlPolicy<StorageT, Type, InitialValue, Index>;
};

// Control trait
template <typename T> struct is_control : std::false_type {};
template <typename S, pcm_control_t C, pcm_uint V, pcm_uint I>
struct is_control<ControlPolicy<S, C, V, I>> : std::true_type {};
template <typename T> inline constexpr bool is_control_v = is_control<std::decay_t<T>>::value;

// ============================================================================
// Stateless signal object policy
// ============================================================================

using GetTimeFn = pcm_uint (*)();
enum class GetTimeBackend { kStd, kHtsim };

uint64_t get_time_diff(pcm_uint ts_start, pcm_uint ts_end) { return ts_end - ts_start; }

template <typename StorageT, pcm_signal_t Type, pcm_uint Mask, pcm_signal_accum_t Accum,
          pcm_uint TriggerThreshold>
struct SignalPolicy {
    static_assert(std::has_single_bit(Mask), "Signal mask must be exactly one bit.");
    static constexpr pcm_signal_t kType = Type;
    static constexpr pcm_uint kMask = Mask;
    static constexpr pcm_uint kIndex = std::countr_zero(Mask);
    static constexpr pcm_uint kInitialTriggerThreshold = TriggerThreshold;
    static constexpr bool kIsTrigger =
        ((TriggerThreshold > 0) && (TriggerThreshold != PCM_SIG_NO_TRIGGER));
    static constexpr bool kIsAccum = (Accum != PCM_SIG_ACCUM_UNSPEC);

    static void update(pcm_uint start_ts, GetTimeFn get_time, StorageT &cur, pcm_uint update_value)
        requires(Accum != PCM_SIG_ACCUM_UNSPEC)
    {
        const StorageT v = static_cast<StorageT>(update_value);
        if constexpr (Type == PCM_SIG_ELAPSED_TIME) {
            if constexpr (kIsTrigger) {
                /* this is a timer and handled in the TriggerSignal, nothing to do */
            } else {
                cur = get_time_diff(start_ts, get_time());
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
            static_assert(always_false_v<void>, "Unsupported accumulator");
        }
    }

    static bool is_fired(pcm_uint start_ts, GetTimeFn get_time, const StorageT &cur,
                         const StorageT &thresh)
        requires(TriggerThreshold != PCM_SIG_NO_TRIGGER)
    {
        if constexpr (Type == PCM_SIG_ELAPSED_TIME) {
            auto timer = cur;
            if (timer) {
                pcm_uint now = get_time_diff(start_ts, get_time());
                /* we assume that now is always larger than timer value */
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

    static void rearm_if_needed(pcm_uint start_ts, GetTimeFn get_time, StorageT &cur)
        requires(TriggerThreshold != PCM_SIG_NO_TRIGGER)
    {
        if constexpr (kType == PCM_SIG_ELAPSED_TIME) {
            if (cur == PCM_SIG_REARM) {
                cur = get_time_diff(start_ts, get_time());
            }
        } else if constexpr (kType == PCM_SIG_DATA_TX) {
            if (cur == PCM_SIG_REARM) {
                cur = static_cast<StorageT>(1);
            }
        }
    }
};

// user-facing signal descriptor -> rebind to policy
template <pcm_signal_t Type, pcm_uint Mask, pcm_signal_accum_t Accum, pcm_uint TriggerThreshold>
struct SignalDesc {
    template <typename StorageT>
    using Rebind = SignalPolicy<StorageT, Type, Mask, Accum, TriggerThreshold>;
};

// Signal trait
template <typename T> struct is_signal : std::false_type {};
template <typename S, pcm_signal_t T, pcm_uint M, pcm_signal_accum_t A, pcm_uint Thr>
struct is_signal<SignalPolicy<S, T, M, A, Thr>> : std::true_type {};
template <typename T> inline constexpr bool is_signal_v = is_signal<std::decay_t<T>>::value;

// ============================================================================
// Flow (type-iterating; no runtime control/signal objects)
// ============================================================================

// ============================================================================
// Flow descriptor (type-erased base)
// ============================================================================
struct FlowDesc {
    virtual ~FlowDesc() = default;
    virtual bool invoke_cc_algorithm_on_trigger() = 0;
};

template <typename Tuple>
concept HasDenseIndices = []<typename... Ts>(std::type_identity<std::tuple<Ts...>>) {
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
        std::conditional_t<is_variable_v<Objs>, std::tuple<Objs>, std::tuple<>>{}...));
    using ControlsTupleT = decltype(std::tuple_cat(
        std::conditional_t<is_control_v<Objs>, std::tuple<Objs>, std::tuple<>>{}...));
    using SignalsTupleT = decltype(std::tuple_cat(
        std::conditional_t<is_signal_v<Objs>, std::tuple<Objs>, std::tuple<>>{}...));

    static_assert(HasDenseIndices<VariablesTupleT>,
                  "Duplicate/non-uniform indices among variables");
    static_assert(HasDenseIndices<ControlsTupleT>, "Duplicate/non-uniform indices among controls");
    static_assert(HasDenseIndices<SignalsTupleT>, "Duplicate/non-uniform indices among signals");

    GetTimeFn get_time_{nullptr};
    pcm_uint start_ts_{};

    /*
     * set/get_control() can be called by the datapath, therefore has no compile-time checks
     * if match was found or not, the API intention is to communicate is_found bool as return type
     * so that callee can handle (possibly) erroneous not found case
     */
    template <pcm_control_t Match> [[nodiscard]] bool set_first_match_control(pcm_uint val) {
        return (([&](auto *self) -> bool {
            using T = Objs;
            if constexpr (is_control_v<T>) {
                if (T::kType == Match) {
                    auto &slot =
                        static_cast<FlowImplT *>(self)->template control_slot_impl<T::kIndex>();
                    T::set(slot, val);
                    return true; // Found - triggers short-circuit
                }
            }
            return false; // Not found - continue
        }(this) || ...));
    }

    template <pcm_control_t Match> [[nodiscard]] std::optional<pcm_uint> get_control_first_match() {
        std::optional<pcm_uint> result;
        (([&](auto *self) -> bool {
            using T = Objs;
            if constexpr (is_control_v<T>) {
                if (T::kType == Match) {
                    auto const &slot = static_cast<FlowImplT const *>(self)
                                           ->template control_slot_impl<T::kIndex>();
                    result = T::get(slot);
                    return true; // Found - triggers short-circuit
                }
            }
            return false; // Not found - continue
        }(this) || ...));
        return result;
    }

    template <GetTimeBackend TimeSrc> void init_time() {
        if constexpr (TimeSrc == GetTimeBackend::kStd) {
            get_time_ = []() -> pcm_uint {
                return static_cast<pcm_uint>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::high_resolution_clock::now().time_since_epoch())
                        .count());
            };
        } else if constexpr (TimeSrc == GetTimeBackend::kHtsim) {
            static_assert(always_false_v<GetTimeBackend>, "HTSIM time source is not supported yet");
        } else {
            static_assert(always_false_v<GetTimeBackend>, "Unsupported time source");
        }
        start_ts_ = get_time_();
    }

    template <pcm_signal_t Match> void update_signals(pcm_uint v) {
        (([&](auto *self) {
             using T = Objs;
             if constexpr (is_signal_v<T>) {
                 if constexpr (T::kIsAccum) {
                     if (T::kType == Match) {
                         auto &slot =
                             static_cast<FlowImplT *>(self)->template signal_slot_impl<T::kIndex>();
                         T::update(start_ts_, get_time_, slot, v);
                     }
                 }
             }
         }(this)),
         ...);
    }

    [[nodiscard]] bool invoke_cc_algorithm_on_trigger() override {
        pcm_uint trigger_mask = 0;

        // collect triggers into the mask
        (([&](auto *self) {
             using T = Objs;
             if constexpr (is_signal_v<T>) {
                 if constexpr (T::kIsTrigger) {
                     auto const &val = static_cast<FlowImplT const *>(self)
                                           ->template signal_slot_impl<T::kIndex>();
                     auto const &thresh = static_cast<FlowImplT const *>(self)
                                              ->template thresh_slot_impl<T::kIndex>();
                     if (T::is_fired(start_ts_, get_time_, val, thresh)) {
                         trigger_mask |= T::kMask;
                     }
                 }
             }
         }(this)),
         ...);

        if (!trigger_mask)
            return false;

        static_cast<FlowImplT *>(this)->prepare_pre_trigger_snapshot_impl(trigger_mask);
        (void)static_cast<FlowImplT *>(this)->execute_algorithm_impl();
        static_cast<FlowImplT *>(this)->apply_post_trigger_snapshot_impl();

        // rearm triggers
        (([&](auto *self) {
             using T = Objs;
             if constexpr (is_signal_v<T>) {
                 if constexpr (T::kIsTrigger) {
                     auto &val =
                         static_cast<FlowImplT *>(self)->template signal_slot_impl<T::kIndex>();
                     T::rearm_if_needed(start_ts_, get_time_, val);
                 }
             }
         }(this)),
         ...);

        return true;
    }
};

// ============================================================================
// Simple flow implementation
// ============================================================================

// Primary template
template <typename Tuple> struct SimpleFlow;

template <typename... Ds>
struct SimpleFlow<std::tuple<Ds...>>
    : Flow<SimpleFlow<std::tuple<Ds...>>, typename Ds::template Rebind<pcm_uint>...> {

    using Base = Flow<SimpleFlow<std::tuple<Ds...>>, typename Ds::template Rebind<pcm_uint>...>;

    pcm_cc_algorithm_cb algorithm_cb_{nullptr};
    explicit SimpleFlow(pcm_cc_algorithm_cb algorithm_cb) : algorithm_cb_{algorithm_cb} {
        (([&] {
             using T = typename Ds::template Rebind<pcm_uint>;
             if constexpr (is_signal_v<T>) {
                 if constexpr (T::kIsTrigger) {
                     thresholds_storage_[T::kIndex] = T::kInitialTriggerThreshold;
                 }
             } else if constexpr (is_control_v<T>) {
                 controls_storage_[T::kIndex] = T::kInitialValue;
             } else if constexpr (is_variable_v<T>) {
                 // Snapshot variables can be initialized directly
                 snapshot_.vars[T::kIndex] = std::bit_cast<pcm_uint>(T::kInitialValue);
             }
         }()),
         ...);
    }

    SimpleFlow(const SimpleFlow &) = default; // default deepcopy used in factory
    SimpleFlow &operator=(const SimpleFlow &) = default;

    flow_datapath_snapshot snapshot_{}; // TODO: have cache for the snapshot?

    using ControlsTupleT = typename Base::ControlsTupleT;
    using SignalsTupleT = typename Base::SignalsTupleT;

    static constexpr std::size_t kNumControls = std::tuple_size<ControlsTupleT>::value;
    static constexpr std::size_t kNumSignals = std::tuple_size<SignalsTupleT>::value;

    std::array<pcm_uint, kNumControls> controls_storage_{};
    std::array<pcm_uint, kNumSignals> signals_storage_{};
    std::array<pcm_uint, kNumSignals> thresholds_storage_{};

    [[nodiscard]] pcm_err_t execute_algorithm_impl() {
        if (!algorithm_cb_)
            throw std::runtime_error("CC Algorithm callback is nullptr");
        return algorithm_cb_(&snapshot_);
    }

    void prepare_pre_trigger_snapshot_impl(pcm_uint trigger_mask) {
        snapshot_.trigger_mask = trigger_mask;
        std::memcpy(snapshot_.signals, signals_storage_.data(), kNumSignals * sizeof(pcm_uint));
        std::memset(signals_storage_.data(), 0, kNumSignals * sizeof(pcm_uint));
        std::memcpy(snapshot_.thresholds, thresholds_storage_.data(),
                    kNumSignals * sizeof(pcm_uint));
        std::memcpy(snapshot_.controls, controls_storage_.data(), kNumControls * sizeof(pcm_uint));
    }

    void apply_post_trigger_snapshot_impl() {
        for (std::size_t i = 0; i < kNumSignals; ++i)
            signals_storage_[i] += snapshot_.signals[i];
        std::memcpy(thresholds_storage_.data(), snapshot_.thresholds,
                    kNumSignals * sizeof(pcm_uint));
        std::memcpy(controls_storage_.data(), snapshot_.controls, kNumControls * sizeof(pcm_uint));
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

template <GetTimeBackend TimeSrc> struct Device {
    [[nodiscard]] bool progress() {
        if (active_flows_.empty())
            return false;

        bool triggered = active_flows_[cur_rr_idx_].second->invoke_cc_algorithm_on_trigger();
        cur_rr_idx_ = (cur_rr_idx_ + 1) % active_flows_.size();
        return triggered;
    }

    // Register a flow spec (mask + factory that deep-copies the spec into the newly created
    // flow)
    template <typename FlowT>
    void add_flow_spec_matching_rule(const FlowT &flow_config, uint32_t matching_rule) {
        static_assert(std::is_base_of_v<FlowDesc, FlowT>,
                      "FlowT must derive from FlowDesc (e.g., SimpleFlow<...>)");
        static_assert(std::is_copy_constructible_v<FlowT>);
        static_assert(std::is_move_constructible_v<FlowT>);
        configs_.emplace_back(matching_rule, [flow_config]() -> std::unique_ptr<FlowDesc> {
            auto new_flow = std::make_unique<FlowT>(flow_config);
            new_flow->template init_time<TimeSrc>();
            return new_flow;
        });
    }

    // Create a new flow instance for a matching address and return it for further configuration
    FlowDesc &create_flow(uint32_t new_address) {
        // address has to be unique
        if (std::any_of(active_flows_.begin(), active_flows_.end(),
                        [&](const auto &p) { return p.first == new_address; })) {
            throw std::runtime_error("Flow with this address already exists");
        }

        // find spec and create
        for (const auto &[mask, factory] : configs_) {
            if (new_address & mask) { // keep your matching rule as-is
                auto &new_flow_pair = active_flows_.emplace_back(new_address, factory());
                // ensure scheduler cursor is within bounds
                if (cur_rr_idx_ >= active_flows_.size()) {
                    cur_rr_idx_ = 0;
                }
                return *new_flow_pair.second;
            }
        }

        throw std::runtime_error("No matching flow spec for address");
    }

    // TODO: implement remove_flow()
  private:
    using Factory = std::function<std::unique_ptr<FlowDesc>()>;
    std::vector<std::pair<uint32_t, Factory>> configs_; // mask + flow factory
    std::vector<std::pair<uint32_t, std::unique_ptr<FlowDesc>>> active_flows_{}; // active flows
    std::size_t cur_rr_idx_{0}; // round robin scheduler cursor
};

// ============================================================================
// Dynamic library loading utilities
// ============================================================================

[[nodiscard]] std::optional<std::pair<void *, void *>>
shared_symbol_open(const std::string &lib_name, const std::string &fn_name) {
    void *so_handle = dlopen(lib_name.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!so_handle) {
        std::cerr << "dlopen(" << lib_name << ") failed with " << dlerror() << std::endl;
        return std::nullopt;
    }

    void *fn_ptr = dlsym(so_handle, fn_name.c_str());
    if (!fn_ptr) {
        dlclose(so_handle);
        std::cerr << "dlsym(" << fn_name << ") failed with " << dlerror() << std::endl;
        return std::nullopt;
    }

    return std::make_pair(so_handle, fn_ptr);
}

void shared_symbol_close(void *so_handle) {
    if (so_handle && dlclose(so_handle)) {
        std::cerr << "dlclose() failed with " << dlerror() << std::endl;
    }
}

} // namespace pcm

// ============================================================================
// Demo
// ============================================================================
using namespace pcm;

pcm_cc_algorithm_cb algorithm_cb = nullptr;

inline constexpr pcm_float kFpVar = 42.0;
inline constexpr pcm_int kIntVar = -42;
inline constexpr pcm_uint kUintVar = 42;

using DatapathSpec =
    std::tuple<ControlDesc<PCM_CTRL_CWND, 8192, 0>,
               SignalDesc<PCM_SIG_ACK, (pcm_uint{1} << 0), PCM_SIG_ACCUM_SUM, PCM_SIG_NO_TRIGGER>,
               SignalDesc<PCM_SIG_RTO, (pcm_uint{1} << 1), PCM_SIG_ACCUM_LAST, 10>,
               VariableDesc<pcm_float, kFpVar, 0>, VariableDesc<pcm_int, kIntVar, 1>,
               VariableDesc<pcm_uint, kUintVar, 2>>;

using FlowSpec = SimpleFlow<DatapathSpec>;

void *algorithm_so_library_handle = nullptr;

[[nodiscard]] std::optional<FlowSpec> flow_spec_create(const std::string &algo_name) {
    auto result =
        shared_symbol_open("lib" + algo_name + ".so", std::string(__algorithm_entry_point_symbol));
    if (!result.has_value()) {
        return std::nullopt;
    }

    auto [so_handle, raw_algorithm_ptr] = result.value();
    algorithm_so_library_handle = so_handle;

    return FlowSpec{reinterpret_cast<pcm_cc_algorithm_cb>(raw_algorithm_ptr)};
}

void flow_spec_destroy() {
    if (algorithm_so_library_handle) {
        shared_symbol_close(algorithm_so_library_handle);
        algorithm_so_library_handle = nullptr;
    }
    // FlowSpec returned from the flow_spec_create() gets destroyed automatically
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <algorithm_name>" << std::endl;
        return 1;
    }

    // register a spec (mask 0xFF)
    auto spec_opt = flow_spec_create(argv[1]);
    if (!spec_opt.has_value()) {
        std::cerr << "Failed to create flow spec for algorithm: " << argv[1] << std::endl;
        return 1;
    }

    Device<GetTimeBackend::kStd> dev;
    dev.add_flow_spec_matching_rule(spec_opt.value(), 0xFF);

    // create a flow for some 32-bit "address"
    auto &fdesc = dev.create_flow(0x12);

    // Cast to the concrete type to access the specific API
    auto &f = static_cast<FlowSpec &>(fdesc);

    auto init_cwnd = f.get_control_first_match<PCM_CTRL_CWND>();
    if (!init_cwnd.has_value()) {
        std::cerr << "Failed to get cwnd" << std::endl;
        return 1;
    }
    std::cout << "Init cwnd: " << init_cwnd.value() << std::endl;

    f.update_signals<PCM_SIG_ACK>(5);
    std::cout << "Invoked flow: " << f.invoke_cc_algorithm_on_trigger() << std::endl;
    f.update_signals<PCM_SIG_ACK>(1000);
    std::cout << "Invoked flow: " << f.invoke_cc_algorithm_on_trigger() << std::endl;
    f.update_signals<PCM_SIG_RTO>(11);
    std::cout << "Invoked flow: " << f.invoke_cc_algorithm_on_trigger() << std::endl;

    auto new_cwnd = f.get_control_first_match<PCM_CTRL_CWND>().value_or(0);
    std::cout << "New cwnd: " << new_cwnd << std::endl;

    auto found = f.set_first_match_control<PCM_CTRL_CWND>(123);
    assert(found);
    auto new2_cwnd = f.get_control_first_match<PCM_CTRL_CWND>().value_or(0);
    std::cout << "Last cwnd: " << new2_cwnd << std::endl;
}
