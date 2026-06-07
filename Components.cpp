#include "Components.hpp"
#include "TimingWheel.hpp"

Component::Component(std::string name, int numReceivers, int numDrivers,
                     int propagationDelay)
    : name(std::move(name)), propagationDelay(propagationDelay)
{
    for (int i = 0; i < numReceivers; ++i)
        receivers.push_back(new Receiver(this, propagationDelay));
    for (int i = 0; i < numDrivers; ++i)
        drivers.push_back(new Driver(this));
}

Component::~Component()
{
    for (Driver*   d : drivers)   delete d;
    for (Receiver* r : receivers) delete r;
}

void Component::scheduleUpdate(int delay)
{
    if (wheel)
        wheel->scheduleComponent(this, delay);
}
