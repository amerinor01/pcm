#pragma once

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "config.h"
#include "eventlist.h"

#include "../impl.h"

namespace pcm {

class DeviceException final : public std::runtime_error {
  public:
    explicit DeviceException(std::string_view message)
        : std::runtime_error(std::string{"Htsim PCM Error: "} + std::string{message}) {
        std::cerr << what() << std::endl;
        exit(EXIT_FAILURE);
    }
};

class Device final : public EventSource {

    // expose time to htsim datapath plugin in PCM
  public:
    [[nodiscard]] static uint64_t getSimulationTime() noexcept;

  private:
    static void setEventList(EventList *eventList);

  public:
    explicit Device(EventList &eventList, std::string_view pcmAlgoName,
                    simtime_picosec handlerDelay, simtime_picosec pollDelay)
        : EventSource{eventList, "Device"}, _pcm_algo_name{pcmAlgoName},
          _handler_delay{handlerDelay}, _poll_delay{pollDelay} {

        setEventList(&eventList);

        if (device_init("htsim", &_pcm_device_ptr) != PCM_SUCCESS)
            throw DeviceException{"Failed to initialize PCM device"};

        if (device_pcmc_init(_pcm_device_ptr, _pcm_algo_name.c_str(), &_pcm_algo_handler) !=
            PCM_SUCCESS)
            throw DeviceException{"Failed to initialize PCMC with algorithm " + _pcm_algo_name};

        _next_sched = eventlist().now() + _poll_delay;
        eventlist().sourceIsPending(*this, _next_sched);
    }

    ~Device() {
        // We can't throw exceptions from this constructor
        if (device_pcmc_destroy(_pcm_algo_handler) != PCM_SUCCESS) {
            std::cerr << "Failed to destroy PCMC handler" << std::endl;
            exit(EXIT_FAILURE);
        }
        if (device_destroy(_pcm_device_ptr) != PCM_SUCCESS) {
            std::cout << "Failed to destroy PCM device" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    void doNextEvent() {
        if (eventlist().now() != _next_sched)
            throw DeviceException{"Current time is not equal to the _next_sched time"};

        _next_sched = eventlist().now() + _poll_delay; // penalize call to sched progress
        int progress_result = device_scheduler_progress(_pcm_device_ptr);
        if (progress_result < 0)
            throw DeviceException{"Failed to progress device scheduler"};
        if (progress_result)
            _next_sched += _handler_delay; // if handler execution happened,
                                           // penalize it as well

        // Scheduler clocks itself
        eventlist().sourceIsPending(*this, _next_sched);
    }

    void stopScheduling() noexcept {
        _next_sched = 0;
        eventlist().cancelPendingSource(*this);
    }

    [[nodiscard]] pcm_device_t getDevicePtr() const noexcept { return _pcm_device_ptr; }

  private:
    simtime_picosec _handler_delay;
    simtime_picosec _poll_delay;
    pcm_device_t _pcm_device_ptr;
    pcm_handle_t _pcm_algo_handler;
    std::string _pcm_algo_name;
    simtime_picosec _next_sched;
};

class Flow final { // Prevent inheritance if not intended
  public:
    explicit Flow(std::shared_ptr<const Device> device) {
        if (flow_create(device->getDevicePtr(), &_pcm_flow_ptr, nullptr) != PCM_SUCCESS) {
            throw DeviceException{"Failed to create flow"};
        }
    }

    Flow(const Flow &) = delete;
    Flow &operator=(const Flow &) = delete;
    Flow(Flow &&) = delete;
    Flow &operator=(Flow &&) = delete;

    ~Flow() noexcept(false) {
        if (flow_destroy(_pcm_flow_ptr) != PCM_SUCCESS) {
            throw DeviceException{"Failed to destroy flow"};
        }
    }

    [[nodiscard]] pcm_uint cwndGet() const noexcept { return flow_cwnd_get(_pcm_flow_ptr); }

    void cwndReset(pcm_uint new_cwnd) noexcept { __flow_control_set(_pcm_flow_ptr, 0, new_cwnd); }

    void signalUpdate(pcm_signal_t sig, pcm_uint val) noexcept {
        flow_signals_update(_pcm_flow_ptr, sig, val);
    }

  private:
    pcm_flow_t _pcm_flow_ptr{};
};

} // namespace pcm