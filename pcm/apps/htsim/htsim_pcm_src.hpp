#pragma once

#include <algorithm>
#include <ranges>

#include "uec.h"

#include "network.h"
#include "pcm_vm.hpp"
#include "prof.h"

namespace pcm_htsim {

class PcmScheduler;
class PcmNic;
class PcmSrc;

class PcmScheduledContext {
  public:
    virtual void fetchUpdate() = 0;
    virtual bool isFinished() = 0;
};

class PcmScheduler final : public EventSource {
  public:
    using PcmVmTag = uint32_t;
    using PcmVmId = uint32_t;
    static const uint64_t default_handlerDelayPs = 1000;
    static const uint64_t default_schedulerPollDelayPs = 1000;
    enum class ProgressType { SYNC, ASYNC };

  public:
    PcmScheduler(EventList &eventList, simtime_picosec handlerDelay,
                 simtime_picosec pollDelay, ProgressType schedType)
        : EventSource(eventList, "PcmScheduler"), _event_list{eventList},
          _handler_delay{handlerDelay}, _poll_delay{pollDelay},
          _sched_type{schedType} {

        if (_sched_type == ProgressType::ASYNC) {
            _next_sched = eventList.now() + _poll_delay;
            _event_list.sourceIsPending(*this, _next_sched);
        }
    }

    [[nodiscard]] ProgressType schedulerTypeGet() const noexcept {
        return _sched_type;
    }

    void attachAlgorithm(std::string_view pcm_algo_name,
                         PcmVmTag vm_matching_rule) {
        // Open shared object that contains PCM VM factory function
        std::string spec_lib_name =
            "lib" + std::string{pcm_algo_name} + "_spec.so";
        std::string vm_factory_fn_name =
            "__" + std::string{pcm_algo_name} + "_spec_get";

        auto symbol_result =
            pcm_vm::util::shared_symbol_open(spec_lib_name, vm_factory_fn_name);
        if (!symbol_result.has_value()) {
            throw std::runtime_error{
                "Failed to load PCM algorithm library: " + spec_lib_name +
                " or find factory function."};
        }

        auto [so_handle, raw_fn_ptr] = symbol_result.value();

        // Store the shared object handle for cleanup
        _spec_so_handle = std::shared_ptr<void>(so_handle, [](void *handle) {
            if (handle) {
                pcm_vm::util::shared_symbol_close(handle);
            }
        });

        // Cast the function pointer to the expected factory function type
        auto vm_factory_fn_ptr =
            reinterpret_cast<pcm_vm::PcmHandlerVmDesc *(*)()>(raw_fn_ptr);

        configs_.emplace_back(
            vm_matching_rule, pcm_algo_name, [vm_factory_fn_ptr]() -> PcmVmPtr {
                PcmVmPtr new_vm(vm_factory_fn_ptr());
                new_vm->add_get_time_source(EventList::getTheEventList().now);
                return new_vm;
            });
    }

    std::pair<PcmVmId, pcm_vm::PcmHandlerVmDesc &>
    createVm(PcmScheduledContext *sched_ctx, PcmVmTag tag) {
        auto new_vm_id = _vm_id_count++;

        if (configs_.size() < 1)
            throw std::runtime_error(
                "Attempt to create a VM before attaching any algorithms");

        // Find matching specification and create
        for (const auto &[matching_rule, algo_name, vm_factory] : configs_) {
            if (matching_rule == tag) {
                std::cout << "PcmScheduler: matched tag " << tag
                          << " to algorithm " << algo_name << std::endl;
                auto ret = _vms_storage.insert(
                    {new_vm_id, std::make_pair(vm_factory(), sched_ctx)});
                if (!ret.second)
                    throw std::runtime_error("New VM insertion failed");
                // Reset scheduling
                cur_rr_it_ = _vms_storage.begin();
                return {new_vm_id, *(ret.first->second.first)};
            }
        }

        throw std::runtime_error("No matching vm spec for id");
    }

    [[nodiscard]] bool pollVm(PcmVmId id) {
        if (_vms_storage.empty())
            throw std::runtime_error("Device VM storage is empty");

        auto it = _vms_storage.find(id);
        if (it == _vms_storage.end())
            throw std::runtime_error("id doesn't exist in vm storage");

        auto triggered = it->second.first->invoke_cc_algorithm_on_trigger();
        if (triggered)
            return true;

        return false;
    }

    void doNextEvent() {
        assert(schedulerTypeGet() == ProgressType::ASYNC);
        if (eventlist().now() != _next_sched)
            throw std::runtime_error{
                "Current time is not equal to the _next_sched time"};
        // Penalize call to sched progress
        _next_sched = eventlist().now() + _poll_delay;

        auto address = pollVmsRoundRobin();
        if (address.has_value()) {
            auto it = _vms_storage.find(address.value());
            if (it == _vms_storage.end()) [[unlikely]]
                throw std::runtime_error{"vm not found in mapping"};
            auto scheduled_ctx = it->second.second;
            if (scheduled_ctx) {
                scheduled_ctx->fetchUpdate();
            }
        }

        // If handler execution happened, penalize it as well
        if (address.has_value())
            _next_sched += _handler_delay;

        // Check if scheduled context finished and stop re-scheduling polling
        // if all context stopped
        size_t num_finished_vms = 0;
        for (auto &[id, vm_ctx_pair] : _vms_storage)
            num_finished_vms +=
                vm_ctx_pair.second && vm_ctx_pair.second->isFinished();
        if (static_cast<std::size_t>(num_finished_vms) == _vms_storage.size()) {
            std::cout << "PcmScheduler finished async VM processing"
                      << std::endl;
            return;
        }

        eventlist().sourceIsPending(*this, _next_sched);
    }

  private:
    [[nodiscard]] std::optional<PcmVmId> pollVmsRoundRobin() {
        assert(schedulerTypeGet() == ProgressType::ASYNC);
        if (_vms_storage.empty())
            return std::nullopt;

        std::optional<PcmVmId> trigger_id = std::nullopt;
        auto triggered = pollVm(cur_rr_it_->first);
        if (triggered)
            trigger_id = cur_rr_it_->first;

        ++cur_rr_it_;
        if (cur_rr_it_ == _vms_storage.end())
            cur_rr_it_ = _vms_storage.begin();
        return trigger_id;
    }

  private:
    EventList &_event_list;
    simtime_picosec _handler_delay;
    simtime_picosec _poll_delay;
    ProgressType _sched_type;
    simtime_picosec _next_sched;
    std::shared_ptr<void> _spec_so_handle;
    using PcmVmPtr = std::unique_ptr<pcm_vm::PcmHandlerVmDesc>;
    std::vector<
        std::tuple<PcmVmTag, std::string_view, std::function<PcmVmPtr()>>>
        configs_;
    using PcmHandlerVmStorage =
        std::unordered_map<PcmVmId, std::pair<PcmVmPtr, PcmScheduledContext *>>;
    PcmHandlerVmStorage _vms_storage{};
    PcmVmId _vm_id_count{0};
    PcmHandlerVmStorage::iterator cur_rr_it_{_vms_storage.end()};
};

class PcmSrc final : public UecSrc, public PcmScheduledContext {

  public:
    PcmSrc(TrafficLogger *trafficLogger, EventList &eventList,
           unique_ptr<UecMultipath> mp, UecNIC &nic, uint32_t no_of_ports,
           PcmScheduler &scheduler, PcmScheduler::PcmVmTag tag,
           bool rts = false)
        : UecSrc{trafficLogger, eventList,   std::move(mp),
                 nic,           no_of_ports, rts},
          _scheduler{scheduler}, _pcm_vm{_scheduler.createVm(this, tag)},
          _pcm_io_slab{_pcm_vm.second.get_signal_io_slab()} {

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

    void fetchUpdate() override {
        PCM_PERF_PROF_REGION_SCOPE_INIT(ctrl_fetch_cycle,
                                        "CONTROL FETCH CYCLE");
        PCM_PERF_PROF_REGION_START(ctrl_fetch_cycle);
        _pcm_vm.second.fetch_slab_output();
        UecSrc::_cwnd = _pcm_io_slab.out.cwnd;
        UecSrc::set_cwnd_bounds();
        PCM_PERF_PROF_REGION_END(ctrl_fetch_cycle, true);

#ifdef ENABLE_PROFILING
        // We have this profiling in fetchUpdate just to allow collecting
        // many samples
        PCM_PERF_PROF_REGION_SCOPE_INIT(call_test_cycle, "RUNTIME CALL TEST");
        PCM_PERF_PROF_REGION_START(call_test_cycle);
        _runtime_call_perftest = _pcm_vm.second.vcall_overhead_test();
        PCM_PERF_PROF_REGION_END(call_test_cycle, true);
        std::cerr << "Side effect for vcall overhead profiling: "
                  << _runtime_call_perftest << std::endl; // add side effect
        PCM_PERF_PROF_REGION_SCOPE_INIT(perf_overhead_cycle,
                                        "PERF OVERHEAD TEST");
        PCM_PERF_PROF_REGION_START(perf_overhead_cycle);
        _runtime_call_perftest = UecSrc::_cwnd;
        PCM_PERF_PROF_REGION_END(perf_overhead_cycle, true);
        std::cerr << "Side effect for profiling overhead: "
                  << _runtime_call_perftest << std::endl; // add side effect
#endif
    }

    bool isFinished() override { return isTotallyFinished(); }

    mem_b datapathCwndGet() const { return UecSrc::_cwnd; }

  private:
    void updateCwndOnAck(bool skip, simtime_picosec delay,
                         mem_b newly_acked_bytes) {
        // std::cout << "pcm_vm::updateCwndOnAck acked_bytes=" <<
        // newly_acked_bytes
        // << std::endl;
        (void)delay; // if needed, queuing delay is computed on the handler
                     // side, RTT sample is delivered instead
        PCM_PERF_PROF_REGION_SCOPE_INIT(ack_registration_cycle,
                                        "ACK REGISTRATION CYCLE");
        PCM_PERF_PROF_REGION_START(ack_registration_cycle);
        _pcm_io_slab.in.ack = 1;
        _pcm_io_slab.in.ecn = skip ? 1 : 0;
        _pcm_io_slab.in.data_tx = newly_acked_bytes;
        _pcm_io_slab.in.rtt = UecSrc::_raw_rtt;
        _pcm_io_slab.in.in_flight = UecSrc::_in_flight;
        _pcm_io_slab.in.tx_backlog_bytes = UecSrc::_backlog;
        _pcm_vm.second.flush_slab_input();
        PCM_PERF_PROF_REGION_END(ack_registration_cycle, true);
        if (_scheduler.schedulerTypeGet() == PcmScheduler::ProgressType::SYNC) {
            if (_scheduler.pollVm(_pcm_vm.first)) {
                fetchUpdate();
            }
        }
    }

    void updateCwndOnNack(bool skip, mem_b nacked_bytes, bool last_hop) {
        // Note: skip and last_hop parameters are unused in PCM implementation
        // for now
        (void)skip;     // Suppress unused parameter warning
        (void)last_hop; // Suppress unused parameter warning

        // std::cout << "pcm_vm::updateCwndOnNack nacked_bytes=" << nacked_bytes
        // << std::endl;
        PCM_PERF_PROF_REGION_SCOPE_INIT(nack_registration_cycle,
                                        "NACK REGISTRATION CYCLE");
        PCM_PERF_PROF_REGION_START(nack_registration_cycle);
        _pcm_io_slab.in.nack = 1;
        _pcm_io_slab.in.data_nacked = nacked_bytes;
        _pcm_io_slab.in.rtt = UecSrc::_base_rtt + UecSrc::_network_rtt;
        _pcm_vm.second.flush_slab_input();
        PCM_PERF_PROF_REGION_END(nack_registration_cycle, true);
        if (_scheduler.schedulerTypeGet() == PcmScheduler::ProgressType::SYNC) {
            if (_scheduler.pollVm(_pcm_vm.first)) {
                fetchUpdate();
            }
        }
    }

  private:
    PcmScheduler &_scheduler;
    std::pair<PcmScheduler::PcmVmId, pcm_vm::PcmHandlerVmDesc &> _pcm_vm;
    pcm_vm::PcmHandlerVmDesc::PcmHandlerVmIoSlab &_pcm_io_slab;
    pcm_uint _runtime_call_perftest;
};

} // namespace pcm_htsim