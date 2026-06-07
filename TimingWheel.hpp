#pragma once
#include <array>
#include <unordered_set>
#include <cstdint>

class Component;
class Clock;

// ─── TimingSlot ───────────────────────────────────────────────────────────────
// One slot in the circular timing wheel.
// Clocks are processed before gate components so their toggled outputs are
// visible to combinational logic in the same tick.

struct TimingSlot
{
    std::unordered_set<Component*> components;
    std::unordered_set<Clock*>     clocks;

    bool empty() const { return components.empty() && clocks.empty(); }
    void clear() { components.clear(); clocks.clear(); }
};

// ─── TimingWheel ──────────────────────────────────────────────────────────────
// Circular array of SIZE slots.  The current slot index represents "now".
// Scheduling with delay D puts the event in slot (current + D) % SIZE.
//
// Invariant: delay must be in [1, SIZE-1].

class TimingWheel
{
public:
    static constexpr int SIZE = 256; // power-of-two for cheap modulo

    TimingWheel() = default;

    // Schedule a gate/combinational component to re-evaluate in `delay` ticks
    void scheduleComponent(Component* c, int delay = 1);

    // Schedule a clock event to fire in `delay` ticks
    void scheduleClock(Clock* c, int delay);

    // Advance one tick: process the current slot, then advance the index.
    // Returns the number of events processed.
    int tick();

    // True if any slot in the wheel has pending events
    bool hasPendingEvents() const;

    // Clear all pending events and reset the tick counter
    void reset();

    uint64_t getCurrentTick()  const { return currentTick; }
    int      getCurrentIndex() const { return currentIndex; }

private:
    std::array<TimingSlot, SIZE> wheel{};
    int      currentIndex = 0;
    uint64_t currentTick  = 0;
};
