#pragma once

#include "uec.h"

#include "htsim_pcm_nic.hpp"
#include "network.h"
#include "pcm_network.h"

namespace pcm {

class Src final : public UecSrc {

  public:
    Src(TrafficLogger *trafficLogger, EventList &eventList,
        unique_ptr<UecMultipath> mp, pcm::Nic &nic, uint32_t no_of_ports,
        bool rts = false)
        : UecSrc{trafficLogger, eventList,   std::move(mp),
                 nic,           no_of_ports, rts},
          _pcm_flow{nic._pcm_device} {

        nic._pcm_device.registerFlow(_pcm_flow.getImplPtr(), this);

        // Assign PCM function pointers for congestion control callbacks
        // Use proper member function pointer assignment syntax
        UecSrc::updateCwndOnAck =
            static_cast<void (UecSrc::*)(bool, simtime_picosec, mem_b)>(
                &Src::updateCwndOnAck);
        UecSrc::updateCwndOnNack =
            static_cast<void (UecSrc::*)(bool, mem_b, bool)>(
                &Src::updateCwndOnNack);
    }

    virtual ~Src() = default;

    void fetchCwndUpdate() noexcept {
        UecSrc::_cwnd = _pcm_flow.cwndGet();
        UecSrc::set_cwnd_bounds();
    }

  private:
    void updateCwndOnAck(bool skip, simtime_picosec delay,
                         mem_b newly_acked_bytes) {
        (void)delay;
        // It's possible that _cwnd was clamped before entering callback
        // so we fetch it here
        _pcm_flow.cwndReset(UecSrc::_cwnd);
        fetchCwndUpdate();

        _pcm_flow.signalUpdate(PCM_SIG_ECN, skip);
        _pcm_flow.signalUpdate(PCM_SIG_DATA_TX, newly_acked_bytes);
        auto num_acked_packets = std::ceil(newly_acked_bytes / UecSrc::_mss);
        _pcm_flow.signalUpdate(PCM_SIG_ACK, num_acked_packets);
        _pcm_flow.signalUpdate(PCM_SIG_RTT, UecSrc::_raw_rtt);
    }

    void updateCwndOnNack(bool skip, mem_b nacked_bytes, bool last_hop) {
        // Note: skip and last_hop parameters are unused in PCM implementation
        // for now
        (void)skip;     // Suppress unused parameter warning
        (void)last_hop; // Suppress unused parameter warning

        // It's possible that _cwnd was clamped before entering callback
        // so we fetch it here
        _pcm_flow.cwndReset(UecSrc::_cwnd);
        fetchCwndUpdate();
        cout << "[PCM flow=" << this
             << ", NACK]: TIME=" << eventlist().now() / 1000
             << " CWND=" << _cwnd << endl;

        auto num_nacked_packets = std::ceil(nacked_bytes / UecSrc::_mss);
        _pcm_flow.signalUpdate(PCM_SIG_NACK, num_nacked_packets);
    }

  private:
    pcm::Flow _pcm_flow;
};

} // namespace pcm