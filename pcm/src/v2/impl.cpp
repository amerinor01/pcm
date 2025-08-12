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

namespace pcm {

// ============================================================================
// Forward declarations
// ============================================================================
template <typename> inline constexpr bool always_false_v = false;

// ============================================================================
// PCMI-local variable descriptor definitions: the simplest for of object associated with the flow
// ============================================================================

// User-exposed variable API
template <typename dtype_T, const dtype_T &initial_value, pcm_uint index> struct VariableDescr {
    static_assert(sizeof(dtype_T) == sizeof(pcm_uint),
                  "Duplicate/non-uniform indices among variables");
    static constexpr pcm_uint _index = index;
    static constexpr dtype_T _initial_value = initial_value;
    template <typename /*storage_T*/> using rebind = VariableDescr<dtype_T, initial_value, index>;
};

// Variable trait
template <typename T> struct is_variable : std::false_type {};
template <typename D, const D &V, pcm_uint I>
struct is_variable<VariableDescr<D, V, I>> : std::true_type {};
template <typename T> inline constexpr bool is_variable_v = is_variable<std::decay_t<T>>::value;

// ============================================================================
// Stateless control object policy
// ============================================================================
template <typename storage_T, pcm_control_t control_type, pcm_uint initial_value, pcm_uint index>
struct ControlPolicy {
    static constexpr pcm_control_t _type = control_type;
    static constexpr pcm_uint _initial_value = initial_value;
    static constexpr pcm_uint _index = index;

    static void set(storage_T &slot, pcm_uint v) { slot = static_cast<storage_T>(v); }
    static pcm_uint get(const storage_T &slot) { return static_cast<pcm_uint>(slot); }
};

// user-facing control descriptor -> rebind to policy implementation
template <pcm_control_t type, pcm_uint initial_value, pcm_uint index> struct control_descr {
    template <typename storage_T>
    using rebind = ControlPolicy<storage_T, type, initial_value, index>;
};

// Control trait
template <typename T> struct is_control : std::false_type {};
template <typename S, pcm_control_t C, pcm_uint V, pcm_uint I>
struct is_control<ControlPolicy<S, C, V, I>> : std::true_type {};
template <typename T> inline constexpr bool is_control_v = is_control<std::decay_t<T>>::value;

// ============================================================================
// Stateless signal object policy
// ============================================================================

using get_time_fn = pcm_uint (*)();
enum class get_time_backend { std, htsim };
uint64_t get_time_diff(pcm_uint ts_start, pcm_uint ts_end) { return ts_end - ts_start; }

template <typename storage_T, pcm_signal_t type, pcm_uint mask, pcm_signal_accum_t accumulator,
          pcm_uint trigger_threshold>
struct SignalPolicy {
    static_assert(std::has_single_bit(mask), "Signal mask must be exactly one bit.");
    static constexpr pcm_signal_t _type = type;
    static constexpr pcm_uint _mask = mask;
    static constexpr pcm_uint _index = std::countr_zero(mask);
    static constexpr pcm_uint _initial_trigger_threshold = trigger_threshold;
    static constexpr bool is_trigger =
        ((trigger_threshold > 0) && (trigger_threshold != PCM_SIG_NO_TRIGGER));
    static constexpr bool is_accum = (accumulator != PCM_SIG_ACCUM_UNSPEC);

    static void update(pcm_uint start_ts, get_time_fn get_time, storage_T &cur,
                       pcm_uint update_value)
        requires(accumulator != PCM_SIG_ACCUM_UNSPEC)
    {
        const storage_T v = static_cast<storage_T>(update_value);
        if constexpr (type == PCM_SIG_ELAPSED_TIME) {
            if constexpr (is_trigger) {
                /* this is a timer and handled in the TriggerSignal, nothing to do */
            } else {
                cur = get_time_diff(start_ts, get_time());
            }
        } else if constexpr (accumulator == PCM_SIG_ACCUM_SUM) {
            cur += v;
        } else if constexpr (accumulator == PCM_SIG_ACCUM_LAST) {
            cur = v;
        } else if constexpr (accumulator == PCM_SIG_ACCUM_MIN) {
            cur = std::min(cur, v);
        } else if constexpr (accumulator == PCM_SIG_ACCUM_MAX) {
            cur = std::max(cur, v);
        } else {
            static_assert(always_false_v<void>, "Unsupported accumulator");
        }
    }

    static bool is_fired(pcm_uint start_ts, get_time_fn get_time, const storage_T &cur,
                         const storage_T &thresh)
        requires(trigger_threshold != PCM_SIG_NO_TRIGGER)
    {
        if constexpr (type == PCM_SIG_ELAPSED_TIME) {
            auto timer = cur;
            if (timer) {
                pcm_uint now = get_time_diff(start_ts, get_time());
                /* we assume that now is always larger than timer value */
                if (now - timer >= thresh) {
                    return true;
                }
            }
            return false;
        } else if constexpr (type == PCM_SIG_DATA_TX) {
            auto burst_len = cur;
            return burst_len ? (burst_len - 1) >= thresh : false;
        } else {
            return cur >= thresh;
        }
    }

    static void rearm_if_needed(pcm_uint start_ts, get_time_fn get_time, storage_T &cur)
        requires(trigger_threshold != PCM_SIG_NO_TRIGGER)
    {
        if constexpr (_type == PCM_SIG_ELAPSED_TIME) {
            if (cur == PCM_SIG_REARM) {
                cur = get_time_diff(start_ts, get_time());
            }
        } else if constexpr (_type == PCM_SIG_DATA_TX) {
            if (cur == PCM_SIG_REARM) {
                cur = static_cast<storage_T>(1);
            }
        }
    }
};

// user-facing signal descriptor -> rebind to policy
template <pcm_signal_t type, pcm_uint mask, pcm_signal_accum_t accumulator,
          pcm_uint trigger_threshold>
struct SignalDescr {
    template <typename storage_T>
    using rebind = SignalPolicy<storage_T, type, mask, accumulator, trigger_threshold>;
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
struct FlowDescr {
    virtual ~FlowDescr() = default;
    virtual bool invoke_cc_algorithm_on_trigger() = 0;
};

template <typename Tuple>
concept HasDenseIndices = []<typename... Ts>(std::type_identity<std::tuple<Ts...>>) {
    constexpr std::size_t n = sizeof...(Ts);
    std::array<pcm_uint, n> a{static_cast<std::size_t>(Ts::_index)...};
    std::sort(a.begin(), a.end());
    for (pcm_uint i = 0; i < n; ++i)
        if (a[i] != i)
            return false;
    return true;
}(std::type_identity<Tuple>{});

template <typename flow_impl_T, typename... Objs> struct Flow : FlowDescr {
    using variables_tuple_T = decltype(std::tuple_cat(
        std::conditional_t<is_variable_v<Objs>, std::tuple<Objs>, std::tuple<>>{}...));
    using controls_tuple_T = decltype(std::tuple_cat(
        std::conditional_t<is_control_v<Objs>, std::tuple<Objs>, std::tuple<>>{}...));
    using signals_tuple_T = decltype(std::tuple_cat(
        std::conditional_t<is_signal_v<Objs>, std::tuple<Objs>, std::tuple<>>{}...));

    static_assert(HasDenseIndices<variables_tuple_T>,
                  "Duplicate/non-uniform indices among variables");
    static_assert(HasDenseIndices<controls_tuple_T>,
                  "Duplicate/non-uniform indices among controls");
    static_assert(HasDenseIndices<signals_tuple_T>, "Duplicate/non-uniform indices among signals");

    get_time_fn _get_time{nullptr};
    pcm_uint _start_ts;

    /*
     * set/get_control() can be called by the datapath, therefore has no compile-time checks
     * if match was found or not, the API intention is to communicate is_found bool as return type
     * so that callee can handle (possibly) erroneous not found case
     */
    template <pcm_control_t Match> [[nodiscard]] bool set_first_match_control(pcm_uint val) {
        return (([&](auto *self) -> bool {
            using T = Objs;
            if constexpr (is_control_v<T>) {
                if (T::_type == Match) {
                    auto &slot =
                        static_cast<flow_impl_T *>(self)->template control_slot_impl<T::_index>();
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
                if (T::_type == Match) {
                    auto const &slot = static_cast<flow_impl_T const *>(self)
                                           ->template control_slot_impl<T::_index>();
                    result = T::get(slot);
                    return true; // Found - triggers short-circuit
                }
            }
            return false; // Not found - continue
        }(this) || ...));
        return result;
    }

    template <get_time_backend TimeSrc> void init_time() {
        if constexpr (TimeSrc == get_time_backend::std) {
            _get_time = []() -> pcm_uint {
                return static_cast<pcm_uint>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::high_resolution_clock::now().time_since_epoch())
                        .count());
            };
        } else if constexpr (TimeSrc == get_time_backend::htsim) {
            static_assert(always_false_v<get_time_backend>,
                          "HTSIM time source is not supported yet");
        } else {
            static_assert(always_false_v<get_time_backend>, "Unsupported time source");
        }
        _start_ts = _get_time();
    }

    template <pcm_signal_t Match> void update_signals(pcm_uint v) {
        (([&](auto *self) {
             using T = Objs;
             if constexpr (is_signal_v<T>) {
                 if constexpr (T::is_accum) {
                     if (T::_type == Match) {
                         auto &slot = static_cast<flow_impl_T *>(self)
                                          ->template signal_slot_impl<T::_index>();
                         T::update(_start_ts, _get_time, slot, v);
                     }
                 }
             }
         }(this)),
         ...);
    }

    [[nodiscard]] bool invoke_cc_algorithm_on_trigger() override {
        pcm_uint trigger_mask = 0;

        // collect triggers
        (([&](auto *self) {
             using T = Objs;
             if constexpr (is_signal_v<T>) {
                 if constexpr (T::is_trigger) {
                     auto const &val = static_cast<flow_impl_T const *>(self)
                                           ->template signal_slot_impl<T::_index>();
                     auto const &thresh = static_cast<flow_impl_T const *>(self)
                                              ->template thresh_slot_impl<T::_index>();
                     if (T::is_fired(_start_ts, _get_time, val, thresh)) {
                         trigger_mask |= T::_mask;
                     }
                 }
             }
         }(this)),
         ...);

        if (!trigger_mask)
            return false;

        static_cast<flow_impl_T *>(this)->prepare_pre_trigger_snapshot_impl(trigger_mask);
        (void)static_cast<flow_impl_T *>(this)->execute_algorithm_impl();
        static_cast<flow_impl_T *>(this)->apply_post_trigger_snapshot_impl();

        // rearm triggers
        (([&](auto *self) {
             using T = Objs;
             if constexpr (is_signal_v<T>) {
                 if constexpr (T::is_trigger) {
                     auto &val =
                         static_cast<flow_impl_T *>(self)->template signal_slot_impl<T::_index>();
                     T::rearm_if_needed(_start_ts, _get_time, val);
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
    : Flow<SimpleFlow<std::tuple<Ds...>>, typename Ds::template rebind<pcm_uint>...> {

    using base = Flow<SimpleFlow<std::tuple<Ds...>>, typename Ds::template rebind<pcm_uint>...>;

    pcm_cc_algorithm_cb _algorithm_cb{nullptr};
    explicit SimpleFlow(pcm_cc_algorithm_cb algorithm_cb) : _algorithm_cb{algorithm_cb} {
        (([&] {
             using T = typename Ds::template rebind<pcm_uint>;
             if constexpr (is_signal_v<T>) {
                 if constexpr (T::is_trigger) {
                     _thresholds_storage[T::_index] = T::_initial_trigger_threshold;
                 }
             } else if constexpr (is_control_v<T>) {
                 _controls_storage[T::_index] = T::_initial_value;
             } else if constexpr (is_variable_v<T>) {
                 // Snapshot variables can be initialized directly
                 _snapshot.vars[T::_index] = std::bit_cast<pcm_uint>(T::_initial_value);
             }
         }()),
         ...);
    }

    SimpleFlow(const SimpleFlow &) = default; // default deepcopy used in factory
    SimpleFlow &operator=(const SimpleFlow &) = default;

    flow_datapath_snapshot _snapshot{}; // TODO: have cache for the snapshot?

    using controls_tuple_T = typename base::controls_tuple_T;
    using signals_tuple_T = typename base::signals_tuple_T;

    static constexpr std::size_t NUM_CONTROLS = std::tuple_size<controls_tuple_T>::value;
    static constexpr std::size_t NUM_SIGNALS = std::tuple_size<signals_tuple_T>::value;

    std::array<pcm_uint, NUM_CONTROLS> _controls_storage{};
    std::array<pcm_uint, NUM_SIGNALS> _signals_storage{};
    std::array<pcm_uint, NUM_SIGNALS> _thresholds_storage{};

    [[nodiscard]] pcm_err_t execute_algorithm_impl() {
        if (!_algorithm_cb)
            throw std::runtime_error("CC Algorithm callback is nullptr");
        return _algorithm_cb(&_snapshot);
    }

    void prepare_pre_trigger_snapshot_impl(pcm_uint trigger_mask) {
        _snapshot.trigger_mask = trigger_mask;
        std::memcpy(_snapshot.signals, _signals_storage.data(), NUM_SIGNALS * sizeof(pcm_uint));
        std::memset(_signals_storage.data(), 0, NUM_SIGNALS * sizeof(pcm_uint));
        std::memcpy(_snapshot.thresholds, _thresholds_storage.data(),
                    NUM_SIGNALS * sizeof(pcm_uint));
        std::memcpy(_snapshot.controls, _controls_storage.data(), NUM_CONTROLS * sizeof(pcm_uint));
    }

    void apply_post_trigger_snapshot_impl() {
        for (std::size_t i = 0; i < NUM_SIGNALS; ++i)
            _signals_storage[i] += _snapshot.signals[i];
        std::memcpy(_thresholds_storage.data(), _snapshot.thresholds,
                    NUM_SIGNALS * sizeof(pcm_uint));
        std::memcpy(_controls_storage.data(), _snapshot.controls, NUM_CONTROLS * sizeof(pcm_uint));
    }

    template <std::size_t I> pcm_uint &control_slot_impl() {
        static_assert(I < NUM_CONTROLS);
        return _controls_storage[I];
    }
    template <std::size_t I> const pcm_uint &control_slot_impl() const {
        static_assert(I < NUM_CONTROLS);
        return _controls_storage[I];
    }
    template <std::size_t I> pcm_uint &signal_slot_impl() {
        static_assert(I < NUM_SIGNALS);
        return _signals_storage[I];
    }
    template <std::size_t I> const pcm_uint &signal_slot_impl() const {
        static_assert(I < NUM_SIGNALS);
        return _signals_storage[I];
    }
    template <std::size_t I> pcm_uint &thresh_slot_impl() {
        static_assert(I < NUM_SIGNALS);
        return _thresholds_storage[I];
    }
    template <std::size_t I> const pcm_uint &thresh_slot_impl() const {
        static_assert(I < NUM_SIGNALS);
        return _thresholds_storage[I];
    }
};

template <get_time_backend TimeSrc> struct Device {
    [[nodiscard]] bool progress() {
        if (_active_flows.empty())
            return false;

        bool triggered = _active_flows[_cur_rr_idx].second->invoke_cc_algorithm_on_trigger();
        _cur_rr_idx = (_cur_rr_idx + 1) % _active_flows.size();
        return triggered;
    }

    // Register a flow spec (mask + factory that deep-copies the spec into the newly creaded
    // flow)
    template <typename FlowT>
    void add_flow_spec_matching_rule(const FlowT &flow_config, uint32_t matching_rule) {
        static_assert(std::is_base_of_v<FlowDescr, FlowT>,
                      "FlowT must derive from FlowDescr (e.g., SimpleFlow<...>)");
        static_assert(std::is_copy_constructible_v<FlowT>);
        static_assert(std::is_move_constructible_v<FlowT>);
        _configs.emplace_back(matching_rule, [flow_config]() -> std::unique_ptr<FlowDescr> {
            auto new_flow = std::make_unique<FlowT>(flow_config);
            new_flow->template init_time<TimeSrc>();
            return new_flow;
        });
    }

    // Create a new flow instance for a matching address and return it for further configuration
    FlowDescr &create_flow(uint32_t new_address) {
        // address has to be unique
        if (std::any_of(_active_flows.begin(), _active_flows.end(),
                        [&](const auto &p) { return p.first == new_address; })) {
            throw std::runtime_error("Flow with this address already exists");
        }

        // find spec and create
        for (const auto &[mask, factory] : _configs) {
            if (new_address & mask) { // keep your matching rule as-is
                auto &new_flow_pair = _active_flows.emplace_back(new_address, factory());
                // ensure scheduler cursor is within bounds
                if (_cur_rr_idx >= _active_flows.size()) {
                    _cur_rr_idx = 0;
                }
                return *new_flow_pair.second;
            }
        }

        throw std::runtime_error("No matching flow spec for address");
    }

    // TODO: implement remove_flow()
  private:
    using Factory = std::function<std::unique_ptr<FlowDescr>()>;
    std::vector<std::pair<uint32_t, Factory>> _configs; // mask + flow factory
    std::vector<std::pair<uint32_t, std::unique_ptr<FlowDescr>>> _active_flows; // active flows
    std::size_t _cur_rr_idx{0}; // round robin scheduler cursor
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

inline constexpr pcm_float fp_var = 42.0;
inline constexpr pcm_int int_var = -42;
inline constexpr pcm_uint uint_var = 42;

pcm_cc_algorithm_cb algorithm_cb = nullptr;

using DatapathSpec =
    std::tuple<control_descr<PCM_CTRL_CWND, 8192, 0>,
               SignalDescr<PCM_SIG_ACK, (pcm_uint{1} << 0), PCM_SIG_ACCUM_SUM, PCM_SIG_NO_TRIGGER>,
               SignalDescr<PCM_SIG_RTO, (pcm_uint{1} << 1), PCM_SIG_ACCUM_LAST, 10>,
               VariableDescr<pcm_float, fp_var, 0>, VariableDescr<pcm_int, int_var, 1>,
               VariableDescr<pcm_uint, uint_var, 2>>;

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

    Device<get_time_backend::std> dev;
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
