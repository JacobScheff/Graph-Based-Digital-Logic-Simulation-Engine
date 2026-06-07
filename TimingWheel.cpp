#include "TimingWheel.hpp"
#include "Components.hpp"
#include "IO.hpp"          // Clock::onTick

void TimingWheel::scheduleComponent(Component* c, int delay)
{
    if (delay < 1) delay = 1;
    if (delay >= SIZE) delay = SIZE - 1;
    wheel[(currentIndex + delay) % SIZE].components.insert(c);
}

void TimingWheel::scheduleClock(Clock* c, int delay)
{
    if (delay < 1) delay = 1;
    if (delay >= SIZE) delay = SIZE - 1;
    wheel[(currentIndex + delay) % SIZE].clocks.insert(c);
}

int TimingWheel::tick()
{
    TimingSlot& slot = wheel[currentIndex];
    int count = 0;

    // ── Process clocks first (outputs visible to gates in same tick) ──────
    auto clocks = slot.clocks;   // copy – onTick may reschedule into wheel
    slot.clocks.clear();
    for (Clock* clk : clocks) {
        clk->onTick(this);
        ++count;
    }

    // ── Process combinational components ──────────────────────────────────
    auto comps = slot.components; // copy – update() may schedule descendants
    slot.components.clear();
    for (Component* c : comps) {
        c->update();
        ++count;
    }

    currentIndex = (currentIndex + 1) % SIZE;
    ++currentTick;
    return count;
}

bool TimingWheel::hasPendingEvents() const
{
    for (const auto& slot : wheel)
        if (!slot.empty()) return true;
    return false;
}

void TimingWheel::reset()
{
    for (auto& slot : wheel)
        slot.clear();
    currentIndex = 0;
    currentTick  = 0;
}
