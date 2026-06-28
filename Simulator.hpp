#pragma once
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include "State.hpp"
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
    void settleCombinational(int maxPasses = 128);
    void update(double deltaTime);

    void  setTicksPerSecond(double tps) { ticksPerSecond = tps; }
    double getTicksPerSecond()   const  { return ticksPerSecond; }

    void notifyRailNetChanged(Net* net);
    void refreshAllComponents();

    TimingWheel&                    getWheel()      { return wheel; }
    const std::vector<Net*>&        getNets()  const { return nets;  }

    // Used by pins during combinatorial settling passes
    bool  isCombinatorialSettling() const { return combinatorialSettling; }
    void  queueDriverState(Driver* driver, State state);
    State getSettlingNetState(Net* net) const;

private:
    void restoreDriverStates(const std::unordered_map<Driver*, State>& states);
    std::unordered_map<Driver*, State> snapshotDriverStates() const;
    static bool driverMapsEqual(const std::unordered_map<Driver*, State>& a,
                                const std::unordered_map<Driver*, State>& b);
    static bool hasDefinedState(const std::unordered_map<Driver*, State>& states);
    void applyHeldDriverStates();
    void seedXorFeedbackPair();
    bool hasFloatingXorOutputs() const;
    void updateHeldDriverStates();
    bool hasDefinedXorFeedbackIn(const std::unordered_map<Driver*, State>& source) const;
    void restoreXorFeedbackStates(const std::unordered_map<Driver*, State>& source);
    void resolveFloatingXorFeedback();
    void applyTransparentXorLoad();
    bool isXnorFeedbackReceiver(Receiver* r) const;
    Receiver* findXnorFeedInReceiver(Component* xnor) const;

    bool combinatorialSettling = false;
    std::unordered_map<Net*, State>    settlingNetSnapshot;
    std::unordered_map<Driver*, State> pendingDriverStates;
    std::unordered_map<Driver*, State> settleEpochStart;
    std::unordered_map<Driver*, State> heldDriverStates;

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
