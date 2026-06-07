#pragma once
#include <vector>
#include <string>

#include "Pin.hpp"   // Driver + Receiver (full types, not just forward decls)

class TimingWheel;
class Simulator;

// ─── Component ────────────────────────────────────────────────────────────────
// Base class for all simulation components.
// Subclasses implement update() to compute new driver states from receiver states.

class Component
{
public:
    Component(std::string name, int numReceivers, int numDrivers,
              int propagationDelay = 1);
    virtual ~Component();

    // Called by the timing wheel when it's this component's turn to evaluate
    virtual void update() = 0;

    // Called when registered with the simulator (rail-relative refresh)
    virtual void onRegistered() {}

    // Called by a Receiver to schedule this component for a future tick
    void scheduleUpdate(int delay);

    // ── Pin accessors ──────────────────────────────────────────────────────
    Driver*   getDriver(int i)   { return drivers[i];   }
    Receiver* getReceiver(int i) { return receivers[i]; }
    int       numDrivers()   const { return static_cast<int>(drivers.size());   }
    int       numReceivers() const { return static_cast<int>(receivers.size()); }

    // ── Metadata ───────────────────────────────────────────────────────────
    const std::string& getName() const { return name; }

    // ── Timing wheel link ──────────────────────────────────────────────────
    void setTimingWheel(TimingWheel* w) { wheel = w; }
    TimingWheel* getTimingWheel() const { return wheel; }

    void setSimulator(Simulator* s) { simulator = s; }
    Simulator* getSimulator() const { return simulator; }

    int getBusWidth() const { return busWidth; }
    void setBusWidth(int w) { if (w >= 1) busWidth = w; }

    // ── UI position (world-space pixels) ──────────────────────────────────
    float x = 0.f, y = 0.f;

protected:
    std::string           name;
    std::vector<Driver*>  drivers;
    std::vector<Receiver*> receivers;
    TimingWheel*          wheel = nullptr;
    Simulator*            simulator = nullptr;
    int                   propagationDelay;
    int                   busWidth = 1;
};
