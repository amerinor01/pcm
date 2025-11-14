#pragma once

#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

// htsim
#include "config.h"
#include "eventlist.h"
#include "uec.h"

// PCM
#include "impl.h"

namespace pcm {

class Src;

class DeviceException final : public std::runtime_error {
  public:
    explicit DeviceException(std::string_view message)
        : std::runtime_error{std::string{"Htsim PCM Error: "} + std::string{message}} {
        std::cerr << what() << std::endl;
        exit(EXIT_FAILURE); // for now all PCM errors are unrecoverable
    }
};

enum class ProgressType { SCHEDULER_TYPE_SYNC, SCHEDULER_TYPE_ASYNC };

class Device final : public EventSource {
    // expose time to htsim datapath plugin in PCM
  public:
    [[nodiscard]] static uint64_t getSimulationTime();

    static const uint64_t default_handlerDelayPs = 1000;
    static const uint64_t default_schedulerPollDelayPs = 1000;

  public:
    explicit Device(EventList &eventList, std::string_view pcmAlgoName, simtime_picosec handlerDelay,
                    simtime_picosec pollDelay, ProgressType schedType);

    ~Device();

    void doNextEvent() override;
    static void setEventList(EventList *eventList);

    [[nodiscard]] pcm_device_t getImplPtr() const noexcept { return _pcm_device_ptr; }
    [[nodiscard]] ProgressType schedulerTypeGet() noexcept { return _sched_type; }
    void registerFlow(pcm_flow_t pcm_flow_handle, pcm::Src *src) { _flow_to_src_mapping[pcm_flow_handle] = src; }

  private:
    std::string _pcm_algo_name;
    simtime_picosec _handler_delay;
    simtime_picosec _poll_delay;
    pcm_device_t _pcm_device_ptr{};
    pcm_handle_t _pcm_algo_handler{};
    simtime_picosec _next_sched;
    std::unordered_map<pcm_flow_t, pcm::Src *> _flow_to_src_mapping;
    ProgressType _sched_type;
};

class Flow final {
  public:
    explicit Flow(const Device &device) {
        if (flow_create(device.getImplPtr(), &_pcm_flow_ptr, nullptr) != PCM_SUCCESS) {
            throw DeviceException{"Failed to create flow"};
        }
    }

    Flow(const Flow &) = delete;
    Flow &operator=(const Flow &) = delete;
    Flow(Flow &&) = delete;
    Flow &operator=(Flow &&) = delete;

    ~Flow() {
        if (_pcm_flow_ptr and flow_destroy(_pcm_flow_ptr) != PCM_SUCCESS) {
            std::cerr << "Failed to destroy flow" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    [[nodiscard]] pcm_flow_t getImplPtr() const noexcept { return _pcm_flow_ptr; }
    [[nodiscard]] pcm_uint cwndGet() const noexcept { return flow_cwnd_get(_pcm_flow_ptr); }
    void signalUpdate(pcm_signal_t sig, pcm_uint val) noexcept { flow_signals_update(_pcm_flow_ptr, sig, val); }

  private:
    pcm_flow_t _pcm_flow_ptr{};
};

class Nic final : public UecNIC {
    friend pcm::Src;

  public:
    Nic(id_t src_num, EventList &eventList, linkspeed_bps linkspeed, uint32_t ports, std::string_view pcmAlgoName,
        simtime_picosec handlerDelay, simtime_picosec pollDelay, ProgressType schedType)
        : UecNIC{src_num, eventList, linkspeed, ports},
          _pcm_device{eventList, pcmAlgoName, handlerDelay, pollDelay, schedType} {}

    virtual ~Nic() = default;

  private:
    pcm::Device _pcm_device;
};

} // namespace pcm