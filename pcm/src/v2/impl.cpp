#include <algorithm>
#include <bit>
#include <cassert>
#include <iostream>
#include <limits>
#include <tuple>
#include <type_traits>

namespace pcm
{

// ---------- common types ----------
using pcm_uint = unsigned long;
const unsigned long PCM_THRESHOLD_UNSPEC{std::numeric_limits<unsigned long>::max()};
using pcm_signal_t = enum class pcm_signal
{
    ACK,
    RTO,
    NACK,
    ECN,
    RTT,
    DATA_TX,
    DATA_NACKED,
    IN_FLIGHT,
    ELAPSED_TIME
};
using pcm_signal_accum_t = enum class pcm_signal_accum
{
    SUM,
    MAX,
    MIN,
    LAST,
    UNSPEC
};
using pcm_control_t = enum class pcm_control
{
    CWND,
    EV
};
using pcm_err_t = enum class pcm_error
{
    SUCCESS,
    ERROR
};

// TODO: implement as part of flow_impl
using flow_datapath_snapshot = unsigned long; // TODO: use struct flow_datapath snapshot from pcmh.h
flow_datapath_snapshot _snapshot;
using cc_algorithm_cb = pcm_err_t (*)(flow_datapath_snapshot *);
cc_algorithm_cb _algorithm_cb;

// ============================================================================
// Forward declarations
// ============================================================================
template <class storage_T, pcm_signal_t signal_type, pcm_uint mask, pcm_signal_accum_t accumulator,
          pcm_uint trigger_threshold>
struct SignalImpl;
template <class storage_T, pcm_control_t control_type, pcm_uint index> struct ControlImpl;

// ============================================================================
// Signals/controls traits/concepts
// ============================================================================
template <typename T> struct is_signal : std::false_type
{
};
template <typename T> inline constexpr bool is_signal_v = is_signal<std::decay_t<T>>::value;

template <typename T> struct is_control : std::false_type
{
};
template <typename T> inline constexpr bool is_control_v = is_control<std::decay_t<T>>::value;

template <typename T>
concept IsAccumSignal = requires { typename T::accum_signal_trait; };

template <typename T>
concept IsTriggerSignal = requires { typename T::trigger_signal_trait; };

template <class> inline constexpr bool always_false_v = false;

// ============================================================================
// Signals/controls descriptor definitions: contain no runtime state
// ============================================================================
template <pcm_signal_t type, pcm_uint mask, pcm_signal_accum_t accumulator,
          pcm_uint trigger_threshold>
struct SignalDescr
{
    template <class storage_T>
    using rebind = struct SignalImpl<storage_T, type, mask, accumulator, trigger_threshold>;
};
template <pcm_signal_t T, pcm_uint M, pcm_signal_accum_t A, pcm_uint Thr>
struct is_signal<SignalDescr<T, M, A, Thr>> : std::true_type
{
};

template <pcm_control_t type, pcm_uint index> struct control_descr
{
    template <class storage_T> using rebind = struct ControlImpl<storage_T, type, index>;
};
template <pcm_control_t C, pcm_uint I> struct is_control<control_descr<C, I>> : std::true_type
{
};

// ============================================================================
// Signal object implementation
// ============================================================================

template <class storage_T, pcm_signal_t type, pcm_uint mask, pcm_signal_accum_t accumulator,
          pcm_uint trigger_threshold>
struct SignalImpl
{
    using self_t = SignalImpl<storage_T, type, mask, accumulator, trigger_threshold>;

    // runtime state
    storage_T _cur_value{0};
    storage_T _trigger_thresh{static_cast<storage_T>(trigger_threshold)};

    // compile-time metadata (lives on impl where we actually use it)
    static_assert(mask != 0, "Signal mask must be non-zero");
    static constexpr pcm_signal_t _type = type;
    static constexpr pcm_uint _mask = mask;
    static constexpr pcm_uint _index = std::countr_zero(mask);

    static constexpr bool is_accum = (accumulator != pcm_signal_accum::UNSPEC);
    static constexpr bool is_trigger = (trigger_threshold != PCM_THRESHOLD_UNSPEC);

    using accum_signal_trait = std::conditional_t<is_accum, std::true_type, std::false_type>;
    using trigger_signal_trait = std::conditional_t<is_trigger, std::true_type, std::false_type>;

    [[nodiscard]] pcm_uint get() const
    {
        return static_cast<pcm_uint>(_cur_value);
    }

    [[nodiscard]] bool is_fired() const
        requires IsTriggerSignal<self_t>
    {
        if constexpr (_type == pcm_signal::ELAPSED_TIME)
        {
            static_assert(always_false_v<self_t>, "Elapsed time trigger is not supported yet");
            // auto timer = _cur_value;
            // if (timer)
            // {
            //     pcm_uint now = time_diff_fn(flow_ctx->start_ts, time_now_fn());
            //     /* we assume that now is always larger than timer value */
            //     if (now - timer >= _trigger_thresh)
            //     {
            //         return true;
            //     }
            // }
            // return false;
        }
        else if constexpr (_type == pcm_signal::DATA_TX)
        {
            auto burst_len = _cur_value;
            if (burst_len)
            {
                if ((burst_len - 1) >= _trigger_thresh)
                {
                    return true;
                }
            }
            return false;
        }
        else
        {
            return _cur_value >= _trigger_thresh;
        }
    }

    void rearm()
        requires IsTriggerSignal<self_t>
    {
        if constexpr (_type == pcm_signal::ELAPSED_TIME)
        {
            static_assert(always_false_v<self_t>, "Elapsed time trigger is not supported yet");
            // if (_cur_value == PCM_SIG_REARM)
            // {
            //     _cur_value = time_diff_fn(flow_ctx->start_ts, time_now_fn());
            // }
        }
        else if constexpr (_type == pcm_signal::DATA_TX)
        {
            auto burst_len = _cur_value;
            if (burst_len)
            {
                _cur_value = 1;
            }
        }
    }

    void update(pcm_uint update_value)
        requires IsAccumSignal<self_t>
    {
        const storage_T v = static_cast<storage_T>(update_value);
        if constexpr (_type == pcm_signal::ELAPSED_TIME)
        {
            if constexpr (is_trigger)
            {
                /* this is a timer and handled in the IsTriggerSignal, nothing to do */
            }
            else
            {
                //_cur_value = time_diff_fn(flow_ctx->start_ts, time_now_fn());
                static_assert(always_false_v<self_t>, "Elapsed time trigger is not supported yet");
            }
        }
        else if constexpr (accumulator == pcm_signal_accum::SUM)
        {
            _cur_value += v;
        }
        else if constexpr (accumulator == pcm_signal_accum::LAST)
        {
            _cur_value = v;
        }
        else if constexpr (accumulator == pcm_signal_accum::MIN)
        {
            _cur_value = std::min(_cur_value, v);
        }
        else if constexpr (accumulator == pcm_signal_accum::MAX)
        {
            _cur_value = std::max(_cur_value, v);
        }
        else
        {
            static_assert(always_false_v<self_t>, "Unsupported accumulator kind");
        }
    }
};
template <typename S, pcm_signal_t T, pcm_uint M, pcm_signal_accum_t A, pcm_uint Thr>
struct is_signal<SignalImpl<S, T, M, A, Thr>> : std::true_type
{
};

// ============================================================================
// Control object implementation
// ============================================================================

template <typename storage_T, pcm_control_t type, pcm_uint index> struct ControlImpl
{
    storage_T _value{};
    static constexpr pcm_control_t _type = type;
    static constexpr pcm_uint _index = index;

    void set(pcm_uint new_value)
    {
        _value = static_cast<storage_T>(new_value);
    }
    [[nodiscard]] pcm_uint get() const
    {
        return static_cast<pcm_uint>(_value);
    }
};
template <typename S, pcm_control_t C, pcm_uint I>
struct is_control<ControlImpl<S, C, I>> : std::true_type
{
};

// ============================================================================
// Flow logic template
// ============================================================================

template <typename flow_impl_T, typename... Objs> struct Flow
{
    // Build tuples of only signals / only controls
    using signals_tuple_T = decltype(std::tuple_cat(
        std::conditional_t<is_signal_v<Objs>, std::tuple<Objs>, std::tuple<>>{}...));
    using controls_tuple_T = decltype(std::tuple_cat(
        std::conditional_t<is_control_v<Objs>, std::tuple<Objs>, std::tuple<>>{}...));

    signals_tuple_T _signals{};
    controls_tuple_T _controls{};

    template <pcm_control_t Match> [[nodiscard]] bool set_control(pcm_uint val)
    {
        bool found = false;
        ((
             [&, this] {
                 using T = Objs;
                 if constexpr (is_control_v<T>)
                 {
                     if (T::_type == Match)
                     {
                         std::get<T::_index>(_controls).set(val);
                         found = true;
                     }
                 }
             }(),
             0),
         ...);
        return found;
    }

    template <pcm_control_t Match>
    [[nodiscard]] std::tuple<bool, pcm_uint> get_control_first_match()
    {
        bool found = false;
        pcm_uint out{};
        ((
             [&, this] {
                 using T = Objs;
                 if constexpr (is_control_v<T>)
                 {
                     if (T::_type == Match)
                     {
                         out = std::get<T::_index>(_controls).get();
                         found = true;
                     }
                 }
             }(),
             0),
         ...);
        return {found, out};
    }

    template <pcm_signal_t Match> void update_signals(pcm_uint v)
    {
        ((
             [&, this] {
                 using T = Objs;
                 if constexpr (is_signal_v<T>)
                 {
                     if (T::is_accum && T::_type == Match)
                     {
                         std::get<T::_index>(_signals).update(v);
                     }
                 }
             }(),
             0),
         ...);
    }

    [[nodiscard]] bool invoke_cc_algorithm_on_trigger()
    {
        pcm_uint trigger_mask = 0;

        // collect triggers
        ((
             [&, this] {
                 using T = Objs;
                 if constexpr (is_signal_v<T>)
                 {
                     if (T::is_trigger)
                     {
                         auto &trig = std::get<T::_index>(_signals);
                         if (trig.is_fired())
                         {
                             trigger_mask |= T::_mask;
                         }
                     }
                 }
             }(),
             0),
         ...);

        if (!trigger_mask)
            return false;

        static_cast<flow_impl_T *>(this)->prepare_pre_trigger_snapshot_impl(trigger_mask);
        (void)static_cast<flow_impl_T *>(this)->execute_algorithm_impl();
        static_cast<flow_impl_T *>(this)->apply_post_trigger_snapshot_impl();

        // rearm all triggers that fired
        ((
             [&, this] {
                 using T = Objs;
                 if constexpr (is_signal_v<T>)
                 {
                     if (T::is_trigger)
                     {
                         std::get<T::_index>(_signals).rearm();
                     }
                 }
             }(),
             0),
         ...);
        return true;
    }
};

// ============================================================================
// Simple flow implementation
// ============================================================================

// Primary template
template <class Storage, class Tuple> struct SimpleFlow;

// tuple peel
template <class Storage, class... Ds>
struct SimpleFlow<Storage, std::tuple<Ds...>>
    : Flow<SimpleFlow<Storage, std::tuple<Ds...>>, typename Ds::template rebind<Storage>...>
{
    using base =
        Flow<SimpleFlow<Storage, std::tuple<Ds...>>, typename Ds::template rebind<Storage>...>;
    using base::base; // inherit ::base constructors

    void prepare_pre_trigger_snapshot_impl(pcm_uint)
    {
    }
    void apply_post_trigger_snapshot_impl()
    {
    }
    [[nodiscard]] pcm_err_t execute_algorithm_impl()
    {
        return pcm_err_t::SUCCESS;
    }
};

} // namespace pcm

using namespace pcm;

using DatapathSpec = std::tuple<
    control_descr<pcm_control::CWND, 0>,
    SignalDescr<pcm_signal::ACK, (1ul << 0), pcm_signal_accum::SUM, PCM_THRESHOLD_UNSPEC>,
    SignalDescr<pcm_signal::RTO, (1ul << 1), pcm_signal_accum::LAST, 10>>;

using FlowSpec = SimpleFlow<pcm_uint, DatapathSpec>;

int main()
{
    FlowSpec f;
    auto found = f.set_control<pcm_control::CWND>(123);
    assert(found);
    auto init_cwnd = f.get_control_first_match<pcm_control::CWND>();
    std::cout << "Init cwnd: " << std::get<1>(init_cwnd) << std::endl;
    f.update_signals<pcm_signal::ACK>(5);
    std::cout << "Invoked flow: " << f.invoke_cc_algorithm_on_trigger() << std::endl;
    auto new_cwnd = f.get_control_first_match<pcm_control::CWND>();
    std::cout << "New cwnd: " << std::get<1>(new_cwnd) << std::endl;
}