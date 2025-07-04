#include "htsim_pcm_device.hpp"
#include "../util.h"

PcmDevice::PcmDevice(EventList &event_list, simtime_picosec handlerDelay,
                     simtime_picosec pollDelay)
    : EventSource(event_list, "PcmDevice"), _handler_delay(handlerDelay),
      _poll_delay(pollDelay) {

    if (device_init("htsim", &_pcm_device_ptr) != SUCCESS) {
        LOG_FATAL("Failed to init PCM device\n");
    }
    _next_sched = eventlist().now() + _poll_delay;
    //eventlist().sourceIsPending(*this, _next_sched);
}

PcmDevice::~PcmDevice() {
    if (device_destroy(_pcm_device_ptr) != SUCCESS) {
        LOG_FATAL("Failed to destroy PCM device\n");
    }
}

device_t *PcmDevice::getDevicePtr() { return _pcm_device_ptr; }

void PcmDevice::stopScheduling() {
    _next_sched = 0;
    eventlist().cancelPendingSource(*this);
}

void PcmDevice::doNextEvent() {
    assert(eventlist().now() == _next_sched);

    _next_sched =
        eventlist().now() + _poll_delay; // penalize call to sched progress
    if (device_scheduler_progress(_pcm_device_ptr)) {
        _next_sched += _handler_delay; // if handler execution happened,
                                       // penalize it as well
    }

    assert(_next_sched > eventlist().now());
    eventlist().sourceIsPending(*this, _next_sched);
}