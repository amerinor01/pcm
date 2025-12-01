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
    UecPcmMp(bool debug, PcmScheduler &scheduler, PcmScheduler::PcmVmTag tag)
        : UecMultipath(debug), _scheduler{scheduler},
          _pcm_vm_id{_scheduler.createVm(this, tag)},
          _pcm_io_slab{_scheduler.getVm(_pcm_vm_id).get_signal_io_slab()} {
        std::cout << "pcm_htsim::UecPcmMp: initialization completed"
                  << std::endl;
    }

    virtual ~UecPcmMp() = default;

    void processEv(uint16_t path_id, PathFeedback feedback) override {
        // std::cout << "pcm_htsim::UecPcmMp: Process EV: " << path_id <<
        // std::endl;

        switch (feedback) {
        case PATH_GOOD:
            _pcm_io_slab->in.ack = 1;
            _pcm_io_slab->in.ack_ev = static_cast<pcm_uint>(path_id);
            break;
        case PATH_ECN:
            _pcm_io_slab->in.ecn = 1;
            _pcm_io_slab->in.ecn_ev = static_cast<pcm_uint>(path_id);
            break;
        case PATH_NACK:
            _pcm_io_slab->in.nack = 1;
            _pcm_io_slab->in.nack_ev = static_cast<pcm_uint>(path_id);
            break;
        case PATH_TIMEOUT:
            /* RTO is not supported yet */
        default:
            std::cerr << "pcm_htsim::UecPcmMp: unsupported signal" << std::endl;
            assert(false);
        }
        _scheduler.getVm(_pcm_vm_id).flush_slab_input();

        if (_scheduler.schedulerTypeGet() == PcmScheduler::ProgressType::SYNC) {
            bool ret;
            ret = _scheduler.pollVm(_pcm_vm_id);
            assert(ret);
            (void)ret;
        }
    }

    void finalize() override { _is_finished = true; }
    bool isFinished() override { return _is_finished; }
    void fetchUpdate() override {}

    uint16_t nextEntropy(uint64_t seq_sent,
                         uint64_t cur_cwnd_in_pkts) override {
        (void)seq_sent;         // Suppress unused parameter warning
        (void)cur_cwnd_in_pkts; // Suppress unused parameter warning
        _pcm_io_slab->in.tx_ready_pkts = 1;
        _scheduler.getVm(_pcm_vm_id).flush_slab_input();

        if (_scheduler.schedulerTypeGet() == PcmScheduler::ProgressType::SYNC) {
            bool ret;
            ret = _scheduler.pollVm(_pcm_vm_id);
            assert(ret);
            (void)ret;
        }

        _scheduler.getVm(_pcm_vm_id).fetch_slab_output();

        std::cout << "pcm_htsim::UecPcmMp:" << this << " Generate EV: "
                  << static_cast<uint16_t>(_pcm_io_slab->out.ev) << std::endl;
        return static_cast<uint16_t>(
            static_cast<uint16_t>(_pcm_io_slab->out.ev));
    }

  private:
    PcmScheduler &_scheduler;
    PcmScheduler::PcmVmId _pcm_vm_id;
    pcm_vm::PcmHandlerVmDesc::PcmHandlerVmIoSlab *_pcm_io_slab;
    bool _is_finished{false};
};

} // namespace pcm_htsim