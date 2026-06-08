#include "IO.hpp"
#include "TimingWheel.hpp"
#include "PowerRails.hpp"

void Clock::start(TimingWheel* w)
{
    setTimingWheel(w);
    running = true;
    w->scheduleClock(this, halfPeriod);
}

void Clock::onTick(TimingWheel* w)
{
    if (!running || !getSimulator()) return;
    outputOn = !outputOn;
    drivers[0]->setState(outputOn ? driveTrue(*this) : driveFalse(*this));
    w->scheduleClock(this, halfPeriod);
}
