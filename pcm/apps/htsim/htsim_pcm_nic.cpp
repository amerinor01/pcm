#include <algorithm>
#include <limits>

#include "htsim_pcm_nic.hpp"
#include "htsim_pcm_src.hpp"

// expose htsim time to PCM
#include "htsim_pcm_time_wrapper_c.h"
extern "C" uint64_t htsim_now(void) { return pcm::Device::getSimulationTime(); }

namespace pcm {

// File-scope global variable for event list (exposed to PCM runtime)
static EventList *_pcm_root_event_list = nullptr;

uint64_t Device::getSimulationTime() {
    if (!_pcm_root_event_list) [[unlikely]]
        throw DeviceException{"Attempt to get simulation time before device initialization"};
    return _pcm_root_event_list->now();
}

// Internal function to set the event list
void Device::setEventList(EventList *eventList) {
    if (!eventList)
        throw DeviceException{"EventList cannot be null"};
    if (_pcm_root_event_list and _pcm_root_event_list != eventList)
        throw DeviceException{"EventList already set to different instance"};
    _pcm_root_event_list = eventList;
}

Device::Device(EventList &eventList, std::string_view pcmAlgoName, simtime_picosec handlerDelay,
               simtime_picosec pollDelay, DeviceSchedulerType schedType)
    : EventSource{eventList, "PcmDevice"}, _pcm_algo_name{pcmAlgoName}, _handler_delay{handlerDelay},
      _poll_delay{pollDelay}, _sched_type{schedType} {

    setEventList(&eventList);

    if (device_init("htsim", &_pcm_device_ptr) != PCM_SUCCESS)
        throw DeviceException{"Failed to initialize PCM device"};

    if (device_pcmc_init(_pcm_device_ptr, _pcm_algo_name.c_str(), &_pcm_algo_handler) != PCM_SUCCESS)
        throw DeviceException{"Failed to initialize PCMC with algorithm " + _pcm_algo_name};

    if (_sched_type == pcm::DeviceSchedulerType::SCHEDULER_TYPE_ASYNC) {
        _next_sched = eventlist().now() + _poll_delay;
        eventlist().sourceIsPending(*this, _next_sched);
    }
}

Device::~Device() {
    // We can't throw exceptions from this destructor
    if (device_pcmc_destroy(_pcm_algo_handler) != PCM_SUCCESS) {
        std::cerr << "Failed to destroy PCMC handler" << std::endl;
        exit(EXIT_FAILURE);
    }
    if (device_destroy(_pcm_device_ptr) != PCM_SUCCESS) {
        std::cout << "Failed to destroy PCM device" << std::endl;
        exit(EXIT_FAILURE);
    }
}

void Device::doNextEvent() {
    if (_sched_type == pcm::DeviceSchedulerType::SCHEDULER_TYPE_ASYNC) {
        if (eventlist().now() != _next_sched)
            throw DeviceException{"Current time is not equal to the _next_sched time"};
        _next_sched = eventlist().now() + _poll_delay; // penalize call to sched progress
    }

    for (auto &it : _flow_to_src_mapping) {
        auto pcm_src = static_cast<pcm::Src *>(it.second);
        auto cwnd = pcm_src->datapathCwndGet();
        __flow_control_set(it.first, 0, cwnd);
    }

    pcm_flow_t triggered_flow;
    auto triggered = device_scheduler_progress(_pcm_device_ptr, &triggered_flow);
    if (triggered) {
        auto it = _flow_to_src_mapping.find(triggered_flow);
        if (it == _flow_to_src_mapping.end()) [[unlikely]]
            throw DeviceException{"PCM source not found in mapping"};
        auto src = it->second;
        if (!src) [[unlikely]]
            throw DeviceException{"PCM source pointer is null"};
        src->datapathCwndUpdate();
    }

    if (_sched_type == pcm::DeviceSchedulerType::SCHEDULER_TYPE_ASYNC) {
        if (triggered)
            _next_sched += _handler_delay; // if handler execution happened, penalize it as well
        auto num_finished_srcs =
            std::ranges::count_if(_flow_to_src_mapping.begin(), _flow_to_src_mapping.end(),
                                  [](const auto &pair) { return pair.second && pair.second->isTotallyFinished(); });
        if (static_cast<std::size_t>(num_finished_srcs) == _flow_to_src_mapping.size())
            return;
        eventlist().sourceIsPending(*this, _next_sched);
    }
}

} // namespace pcm