#pragma once

#include "uec.h"

#include "htsim_pcm_nic.hpp"
#include "network.h"
#include "pcm_network.h"

namespace pcm {

class Src final : public UecSrc {

  public:
    Src(TrafficLogger *trafficLogger, EventList &eventList, unique_ptr<UecMultipath> mp, pcm::Nic &nic,
        uint32_t no_of_ports, bool rts = false)
        : UecSrc{trafficLogger, eventList, std::move(mp), nic, no_of_ports, rts}, _pcm_flow{nic._pcm_device},
          _pcm_device{nic._pcm_device} {

        nic._pcm_device.registerFlow(_pcm_flow.getImplPtr(), this);

        // Assign PCM function pointers for congestion control callbacks
        // Use proper member function pointer assignment syntax
        UecSrc::updateCwndOnAck = static_cast<void (UecSrc::*)(bool, simtime_picosec, mem_b)>(&Src::updateCwndOnAck);
        UecSrc::updateCwndOnNack = static_cast<void (UecSrc::*)(bool, mem_b, bool)>(&Src::updateCwndOnNack);
    }

    virtual ~Src() = default;

    void datapathCwndUpdate() noexcept {
        UecSrc::_cwnd = _pcm_flow.cwndGet();
        UecSrc::set_cwnd_bounds();
    }

    mem_b datapathCwndGet() const { return UecSrc::_cwnd; }

  private:
    void updateCwndOnAck(bool skip, simtime_picosec delay, mem_b newly_acked_bytes) {
        // std::cout << "PCM::updateCwndOnAck acked_bytes=" << newly_acked_bytes << std::endl;
        (void)delay; // if needed, queuing delay is computed on the handler side, RTT sample is delivered instead

        _pcm_flow.signalUpdate(PCM_SIG_ECN, skip);
        _pcm_flow.signalUpdate(PCM_SIG_DATA_TX, newly_acked_bytes);
        _pcm_flow.signalUpdate(PCM_SIG_ACK, 1);
        _pcm_flow.signalUpdate(PCM_SIG_IN_FLIGHT, UecSrc::_in_flight);
        _pcm_flow.signalUpdate(PCM_SIG_RTT, UecSrc::_raw_rtt);

        if (_pcm_device.schedulerTypeGet() == pcm::DeviceSchedulerType::SCHEDULER_TYPE_SYNC) {
            _pcm_device.doNextEvent();
        }
    }

    void updateCwndOnNack(bool skip, mem_b nacked_bytes, bool last_hop) {
        // Note: skip and last_hop parameters are unused in PCM implementation
        // for now
        (void)skip;     // Suppress unused parameter warning
        (void)last_hop; // Suppress unused parameter warning

        // std::cout << "PCM::updateCwndOnNack nacked_bytes=" << nacked_bytes << std::endl;

        _pcm_flow.signalUpdate(PCM_SIG_NACK, 1);
        _pcm_flow.signalUpdate(PCM_SIG_DATA_NACKED, nacked_bytes);
        _pcm_flow.signalUpdate(PCM_SIG_RTT, UecSrc::_base_rtt + UecSrc::_network_rtt);

        if (_pcm_device.schedulerTypeGet() == pcm::DeviceSchedulerType::SCHEDULER_TYPE_SYNC) {
            _pcm_device.doNextEvent();
        }
    }

  private:
    pcm::Flow _pcm_flow;
    pcm::Device &_pcm_device;
};

} // namespace pcm