#ifndef HTSIM_PCM_DEVICE_HPP
#define HTSIM_PCM_DEVICE_HPP
#include "../impl.h"
#include "config.h"
#include "eventlist.h"

class PcmDevice : public EventSource {
  public:
    PcmDevice(EventList &eventlist, simtime_picosec handlerDelay,
              simtime_picosec pollDelay);
    ~PcmDevice();
    device_t *getDevicePtr();
    void doNextEvent();
    void stopScheduling(); // do we need it?
    void addFlow();
    void removeFlow();
  private:
    simtime_picosec _handler_delay;
    simtime_picosec _poll_delay;
    device_t *_pcm_device_ptr;

    simtime_picosec _next_sched;
};

#endif