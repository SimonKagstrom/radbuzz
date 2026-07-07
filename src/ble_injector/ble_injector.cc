#include "ble_injector.hh"

void BleInjector::PollInjections()
{
    Event ev;

    while (m_event_queue.pop(ev))
    {
        // Presense verified in the Inject method
        OnInjection(ev.uuid, {ev.data.data(), ev.data.size()});
    }
}
