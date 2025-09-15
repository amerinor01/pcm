#pragma once

#include <algorithm>
#include <ranges>

#include "uec.h"

#include "network.h"
#include "pcm_v2.hpp"

namespace pcm_htsim {

class PcmScheduler;
class PcmNic;
class PcmSrc;

class DeviceException final : public std::runtime_error {
  public:
    explicit DeviceException(std::string_view message)
        : std::runtime_error{std::string{"Htsim PCM Error: "} +
                             std::string{message}} {
        std::cerr << what() << std::endl;
        exit(EXIT_FAILURE); // for now all PCM errors are unrecoverable
    }
};

enum class ProgressType { SCHEDULER_TYPE_SYNC, SCHEDULER_TYPE_ASYNC };

class PcmScheduler final : public EventSource {
  public:
    PcmScheduler(EventList &eventList, std::string_view pcmAlgoName,
                 simtime_picosec handlerDelay, simtime_picosec pollDelay,
                 ProgressType schedType)
        : EventSource(eventList, "PcmScheduler"), _event_list{eventList},
          _pcm_algo_name{pcmAlgoName}, _handler_delay{handlerDelay},
          _poll_delay{pollDelay}, _sched_type{schedType},
          _dev{get_current_time} {

        // Open shared object with PCM algorithm and get factory function
        std::string lib_name = "lib" + std::string{_pcm_algo_name} + "_spec.so";
        std::string factory_fn_name =
            "__" + std::string{_pcm_algo_name} + "_spec_get";

        auto symbol_result =
            pcm::util::shared_symbol_open(lib_name, factory_fn_name);
        if (!symbol_result.has_value()) {
            throw DeviceException{
                "Failed to load PCM algorithm library: " + lib_name +
                " or find factory function. Tried: create_" +
                std::string{_pcm_algo_name} + "_spec and " +
                std::string{_pcm_algo_name} + "_factory"};
        }

        auto [so_handle, factory_fn_ptr] = symbol_result.value();

        // Cast the function pointer to the expected factory function type
        using FactoryFn = pcm::FlowDesc *(*)();
        auto factory_fn = reinterpret_cast<FactoryFn>(factory_fn_ptr);

        // Store the shared object handle for cleanup
        _spec_so_handle = std::shared_ptr<void>(so_handle, [](void *handle) {
            if (handle) {
                pcm::util::shared_symbol_close(handle);
            }
        });

        // Register the factory function with the PCM device
        _dev.add_flow_spec_factory(
            [factory_fn]() -> pcm::FlowDesc * {
                if (!factory_fn) {
                    throw std::runtime_error("Factory function is null");
                }
                return factory_fn();
            },
            std::numeric_limits<uint32_t>::max());

        if (_sched_type == ProgressType::SCHEDULER_TYPE_ASYNC) {
            _next_sched = eventList.now() + _poll_delay;
            _event_list.sourceIsPending(*this, _next_sched);
        }
    }
    virtual ~PcmScheduler() = default;

    static pcm_uint get_current_time() {
        return EventList::getTheEventList().now();
    }

    pcm::FlowDesc &createFlow(PcmSrc *src) {
        auto new_flow_addr = flow_addr_count++;
        _flow_addr_to_src_mapping[new_flow_addr] = src;
        return _dev.create_flow(new_flow_addr);
    }

    [[nodiscard]] ProgressType schedulerTypeGet() noexcept {
        return _sched_type;
    }

    void doNextEvent(); // Declaration only - implementation after PcmSrc class

  private:
    EventList &_event_list;
    std::string _pcm_algo_name;
    simtime_picosec _handler_delay;
    simtime_picosec _poll_delay;
    ProgressType _sched_type;
    pcm::DeviceCheckOnSched _dev;
    simtime_picosec _next_sched;
    uint32_t flow_addr_count{0};
    std::unordered_map<uint32_t, PcmSrc *> _flow_addr_to_src_mapping;
    std::shared_ptr<void> _spec_so_handle;
};

class PcmNic final : public UecNIC {
  public:
    static const uint64_t default_handlerDelayPs = 1000;
    static const uint64_t default_schedulerPollDelayPs = 1000;
    PcmNic(id_t src_num, EventList &eventList, linkspeed_bps linkspeed,
           uint32_t ports, std::string_view pcmAlgoName,
           simtime_picosec handlerDelay, simtime_picosec pollDelay,
           ProgressType schedType)
        : UecNIC{src_num, eventList, linkspeed, ports},
          _scheduler{eventList, pcmAlgoName, handlerDelay, pollDelay,
                     schedType} {}
    PcmScheduler _scheduler;
};

class PcmSrc final : public UecSrc {

  public:
    PcmSrc(TrafficLogger *trafficLogger, EventList &eventList,
           unique_ptr<UecMultipath> mp, PcmNic &nic, uint32_t no_of_ports,
           bool rts = false)
        : UecSrc{trafficLogger, eventList,   std::move(mp),
                 nic,           no_of_ports, rts},
          _nic{nic}, _pcm_flow{_nic._scheduler.createFlow(this)} {

        // Assign PCM function pointers for congestion control callbacks
        // Use proper member function pointer assignment syntax
        UecSrc::updateCwndOnAck =
            static_cast<void (UecSrc::*)(bool, simtime_picosec, mem_b)>(
                &PcmSrc::updateCwndOnAck);
        UecSrc::updateCwndOnNack =
            static_cast<void (UecSrc::*)(bool, mem_b, bool)>(
                &PcmSrc::updateCwndOnNack);
    }

    virtual ~PcmSrc() = default;

    void fetchCwndUpdate() noexcept {
        UecSrc::_cwnd =
            _pcm_flow.get_control_first_match_runtime(PCM_CTRL_CWND);
        UecSrc::set_cwnd_bounds();
    }

    mem_b datapathCwndGet() const { return UecSrc::_cwnd; }

  private:
    void updateCwndOnAck(bool skip, simtime_picosec delay,
                         mem_b newly_acked_bytes) {
        // std::cout << "PCM::updateCwndOnAck acked_bytes=" << newly_acked_bytes
        // << std::endl;
        (void)delay; // if needed, queuing delay is computed on the handler
                     // side, RTT sample is delivered instead

        _pcm_flow.update_signals_runtime(PCM_SIG_ECN, skip ? 1 : 0);
        _pcm_flow.update_signals_runtime(PCM_SIG_DATA_TX, newly_acked_bytes);
        _pcm_flow.update_signals_runtime(PCM_SIG_ACK, 1);
        _pcm_flow.update_signals_runtime(PCM_SIG_IN_FLIGHT, UecSrc::_in_flight);
        _pcm_flow.update_signals_runtime(PCM_SIG_RTT, UecSrc::_raw_rtt);

        if (_nic._scheduler.schedulerTypeGet() ==
            ProgressType::SCHEDULER_TYPE_SYNC) {
            _nic._scheduler.doNextEvent();
        }
    }

    void updateCwndOnNack(bool skip, mem_b nacked_bytes, bool last_hop) {
        // Note: skip and last_hop parameters are unused in PCM implementation
        // for now
        (void)skip;     // Suppress unused parameter warning
        (void)last_hop; // Suppress unused parameter warning

        // std::cout << "PCM::updateCwndOnNack nacked_bytes=" << nacked_bytes <<
        // std::endl;

        _pcm_flow.update_signals_runtime(PCM_SIG_NACK, 1);
        _pcm_flow.update_signals_runtime(PCM_SIG_DATA_NACKED, nacked_bytes);
        _pcm_flow.update_signals_runtime(PCM_SIG_RTT, UecSrc::_base_rtt +
                                                          UecSrc::_network_rtt);

        if (_nic._scheduler.schedulerTypeGet() ==
            ProgressType::SCHEDULER_TYPE_SYNC) {
            _nic._scheduler.doNextEvent();
        }
    }

  private:
    PcmNic &_nic;
    pcm::FlowDesc &_pcm_flow;
};

// PcmScheduler method implementations that need complete PcmSrc definition
void PcmScheduler::doNextEvent() {
    if (schedulerTypeGet() == ProgressType::SCHEDULER_TYPE_ASYNC) {
        if (eventlist().now() != _next_sched)
            throw DeviceException{
                "Current time is not equal to the _next_sched time"};
        _next_sched =
            eventlist().now() + _poll_delay; // penalize call to sched progress
    }

    for (auto &it : _flow_addr_to_src_mapping) {
        auto pcm_src = static_cast<PcmSrc *>(it.second);
        auto cwnd = pcm_src->datapathCwndGet();
        // flow_cwnd_set(it.first, cwnd);
    }

    auto address = _dev.progress();
    if (address.has_value()) {
        auto it = _flow_addr_to_src_mapping.find(address.value());
        if (it == _flow_addr_to_src_mapping.end()) [[unlikely]]
            throw DeviceException{"PCM source not found in mapping"};
        auto pcm_src = it->second;
        if (!pcm_src) [[unlikely]]
            throw DeviceException{"PCM source pointer is null"};
        pcm_src->fetchCwndUpdate();
    }

    if (schedulerTypeGet() == ProgressType::SCHEDULER_TYPE_ASYNC) {
        if (address.has_value())
            _next_sched += _handler_delay; // if handler execution happened,
                                           // penalize it as well
        auto num_finished_srcs = std::ranges::count_if(
            _flow_addr_to_src_mapping.begin(), _flow_addr_to_src_mapping.end(),
            [](const auto &pair) {
                return pair.second && pair.second->isTotallyFinished();
            });
        if (static_cast<std::size_t>(num_finished_srcs) ==
            _flow_addr_to_src_mapping.size())
            return;
        eventlist().sourceIsPending(*this, _next_sched);
    }
}

} // namespace pcm_htsim