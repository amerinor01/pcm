#pragma once

#include <algorithm>
#include <ranges>

#include "uec.h"
#include "uec_mp.h"

#include "htsim_pcm_src.hpp"
#include "network.h"

namespace pcm_htsim {

class UecPcmMp : public UecMultipath, public PcmScheduledContext {
  public:
    UecPcmMp(bool debug, PcmScheduler &scheduler)
        : UecMultipath(debug), _scheduler{scheduler},
          _pcm_vm{_scheduler.createVm(this, 0)} {
        std::cout << "pcm_htsim::UecPcmMp: initialization completed"
                  << std::endl;
    }

    virtual ~UecPcmMp() = default;

    void processEv(uint16_t path_id, PathFeedback feedback) override {
        // std::cout << "pcm_htsim::UecPcmMp: Process EV: " << path_id <<
        // std::endl;
        switch (feedback) {
        case PATH_GOOD:
            _pcm_vm.second.update_signals_runtime(PCM_SIG_ACK, 1);
            _pcm_vm.second.update_signals_runtime(
                PCM_SIG_ACK_EV, static_cast<pcm_uint>(path_id));
            break;
        case PATH_ECN:
            _pcm_vm.second.update_signals_runtime(PCM_SIG_ECN, 1);
            _pcm_vm.second.update_signals_runtime(
                PCM_SIG_ECN_EV, static_cast<pcm_uint>(path_id));
            break;
        case PATH_NACK:
            _pcm_vm.second.update_signals_runtime(PCM_SIG_NACK, 1);
            _pcm_vm.second.update_signals_runtime(
                PCM_SIG_NACK_EV, static_cast<pcm_uint>(path_id));
            break;
        case PATH_TIMEOUT:
            /* RTO is not supported yet */
        default:
            std::cerr << "pcm_htsim::UecPcmMp: unsupported signal" << std::endl;
            assert(false);
        }
        if (_scheduler.schedulerTypeGet() ==
            PcmScheduler::ProgressType::SYNC) {
            bool ret;
            ret = _scheduler.pollVm(_pcm_vm.first);
            assert(ret);
            (void)ret;
        }
    }

    void finalize() override { _is_finished = true; }
    bool isFinished() override { return _is_finished; }
    void fetchUpdate() override {}

    uint16_t nextEntropy(uint64_t seq_sent,
                         uint64_t cur_cwnd_in_pkts) override {
        (void)seq_sent;
        (void)cur_cwnd_in_pkts;
        _pcm_vm.second.update_signals_runtime(PCM_SIG_TX_BACKLOG_PKTS, 1);

        if (_scheduler.schedulerTypeGet() ==
            PcmScheduler::ProgressType::SYNC) {
            bool ret;
            ret = _scheduler.pollVm(_pcm_vm.first);
            assert(ret);
            (void)ret;
        }

        std::cout << "pcm_htsim::UecPcmMp:" << this << " Generate EV: "
                  << static_cast<uint16_t>(
                         _pcm_vm.second.get_control_first_match_runtime(
                             PCM_CTRL_EV))
                  << std::endl;
        return static_cast<uint16_t>(
            _pcm_vm.second.get_control_first_match_runtime(PCM_CTRL_EV));
    }

  private:
    PcmScheduler &_scheduler;
    std::pair<PcmScheduler::PcmVmId, pcm_vm::PcmHandlerVmDesc &> _pcm_vm;
    bool _is_finished{false};
};

} // namespace pcm_htsim