#pragma once
#include <vector>
#include <memory>
#include <string>
#include "TimingWheel.hpp"

class Component;
class Net;
class Clock;
class Driver;
class Receiver;
class RailSource;

// ─── Simulator ────────────────────────────────────────────────────────────────
// Owns the timing wheel and all Net objects.
// Components are owned externally (by the Canvas) and merely registered here.

class Simulator
{
public:
    Simulator();
    ~Simulator();

    // ── Component registry ────────────────────────────────────────────────
    void registerComponent(Component* c);
    void unregisterComponent(Component* c);

    void registerClock(Clock* c);
    void unregisterClock(Clock* c);

    // ── Net management ────────────────────────────────────────────────────
    Net* createNet(std::string name = {}, int width = 1);
    void removeNet(Net* net);

    // ── Pin connections ───────────────────────────────────────────────────
    Net* connectDriver(Driver* driver, Net* net = nullptr);
    void connectReceiver(Receiver* receiver, Net* net);
    void disconnectDriver(Driver* driver);
    void disconnectReceiver(Receiver* receiver);

    // Width compatibility check before connecting
    bool canConnect(Driver* driver, Receiver* receiver, Net* net = nullptr) const;
    bool canConnect(Driver* driver, Net* net) const;

    // ── Power rails ───────────────────────────────────────────────────────
    Net* getVddNet() const { return vddNet; }
    Net* getGndNet() const { return gndNet; }

    // ── Simulation control ────────────────────────────────────────────────
    void start();
    void stop();
    void reset();

    bool isRunning() const { return running; }

    void step(int n = 1);
    void settle(int maxTicks = 512);
    void update(double deltaTime);

    void  setTicksPerSecond(double tps) { ticksPerSecond = tps; }
    double getTicksPerSecond()   const  { return ticksPerSecond; }

    void notifyRailNetChanged(Net* net);
    void refreshAllComponents();

    TimingWheel&                    getWheel()      { return wheel; }
    const std::vector<Net*>&        getNets()  const { return nets;  }

private:
    TimingWheel                      wheel;
    std::vector<Component*>          components;
    std::vector<Clock*>              clocks;
    std::vector<Net*>                nets;

    Net*                             vddNet = nullptr;
    Net*                             gndNet = nullptr;
    std::unique_ptr<RailSource>      vddSource;
    std::unique_ptr<RailSource>      gndSource;

    bool   running       = false;
    double ticksPerSecond = 1000.0;
    double accumulated    = 0.0;
};
