#pragma once

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

// htsim
#include "config.h"
#include "eventlist.h"

// PCM
#include "../impl.h"

namespace pcm {

class DeviceException final : public std::runtime_error {
  public:
    explicit DeviceException(std::string_view message)
        : std::runtime_error{std::string{"Htsim PCM Error: "} + std::string{message}} {
        std::cerr << what() << std::endl;
        exit(EXIT_FAILURE);
    }
};

class Device final : public EventSource {

    // expose time to htsim datapath plugin in PCM
  public:
    [[nodiscard]] static uint64_t getSimulationTime();

    static const uint64_t default_handlerDelayPs = 1000;
    static const uint64_t default_schedulerPollDelayPs = 1000;

  public:
    explicit Device(EventList &eventList, std::string_view pcmAlgoName, simtime_picosec handlerDelay,
                    simtime_picosec pollDelay);

    ~Device();

    void doNextEvent();
    void stopScheduling() noexcept {
        _next_sched = 0;
        eventlist().cancelPendingSource(*this);
    }

    [[nodiscard]] pcm_device_t getDevicePtr() const noexcept { return _pcm_device_ptr; }

    static void setEventList(EventList *eventList);

  private:
  private:
    std::string _pcm_algo_name;
    simtime_picosec _handler_delay;
    simtime_picosec _poll_delay;
    pcm_device_t _pcm_device_ptr;
    pcm_handle_t _pcm_algo_handler;
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

    void signalUpdate(pcm_signal_t sig, pcm_uint val) noexcept { flow_signals_update(_pcm_flow_ptr, sig, val); }

  private:
    pcm_flow_t _pcm_flow_ptr{};
};

} // namespace pcm