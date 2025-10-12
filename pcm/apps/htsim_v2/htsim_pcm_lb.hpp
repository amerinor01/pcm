#pragma once

#include <algorithm>
#include <ranges>

#include "uec.h"
#include "uec_mp.h"

#include "htsim_pcm_src.hpp"
#include "network.h"
#include "pcm_v2.hpp"

namespace pcm_htsim {

class UecPcmMp : public UecMultipath {
  public:
    UecPcmMp(bool debug, PcmNic &nic)
        : UecMultipath(debug), _nic{nic},
          _pcm_flow{_nic._scheduler.createFlow(nullptr)} {
        // Note: passing nullptr into createFlow doesn't break scheduler because
        // in SCHEDULER_TYPE_SYNC we rely only on the functionality of scheduler
        // to init pcm::flow according to internal matching rule
        assert(_nic._scheduler.schedulerTypeGet() ==
               ProgressType::SCHEDULER_TYPE_SYNC);
        std::cout << "pcm_htsim::UecPcmMp: initialization completed"
                  << std::endl;
    }

    virtual ~UecPcmMp() = default;

    void processEv(uint16_t path_id, PathFeedback feedback) override {
        // std::cout << "pcm_htsim::UecPcmMp: Process EV: " << path_id <<
        // std::endl;
        switch (feedback) {
        case PATH_GOOD:
            _pcm_flow.second.update_signals_runtime(PCM_SIG_ACK, 1);
            _pcm_flow.second.update_signals_runtime(
                PCM_SIG_ACK_EV, static_cast<pcm_uint>(path_id));
            break;
        case PATH_ECN:
            _pcm_flow.second.update_signals_runtime(PCM_SIG_ECN, 1);
            _pcm_flow.second.update_signals_runtime(
                PCM_SIG_ECN_EV, static_cast<pcm_uint>(path_id));
            break;
        case PATH_NACK:
            _pcm_flow.second.update_signals_runtime(PCM_SIG_NACK, 1);
            _pcm_flow.second.update_signals_runtime(
                PCM_SIG_NACK_EV, static_cast<pcm_uint>(path_id));
            break;
        case PATH_TIMEOUT:
            /* RTO is not supported yet */
        default:
            std::cerr << "pcm_htsim::UecPcmMp: unsupported signal" << std::endl;
            assert(false);
        }
        bool ret;
        ret = _nic._scheduler.progress(_pcm_flow.first);
        assert(ret);
    }

    uint16_t nextEntropy(uint64_t seq_sent,
                         uint64_t cur_cwnd_in_pkts) override {
        _pcm_flow.second.update_signals_runtime(PCM_SIG_TX_BACKLOG_PKTS, 1);
        std::optional<uint32_t> ret = std::nullopt;
        _nic._scheduler.progress(_pcm_flow.first);
        // std::cout << "pcm_htsim::UecPcmMp: Generate EV: "
        //           <<
        //           _pcm_flow.get_control_first_match_runtime(PCM_CTRL_EV)
        //           << std::endl;
        return static_cast<uint16_t>(
            _pcm_flow.second.get_control_first_match_runtime(PCM_CTRL_EV));
    }

  private:
    PcmNic &_nic;
    std::pair<uint32_t, pcm::FlowDesc &> _pcm_flow;
};

} // namespace pcm_htsim