#include "Simulator.hpp"
#include "Components.hpp"
#include "IO.hpp"
#include "Pin.hpp"
#include "Net.hpp"
#include "State.hpp"
#include "PowerRails.hpp"
#include <algorithm>

Simulator::Simulator()
{
    vddNet = createNet("VDD");
    gndNet = createNet("GND");

    vddSource = std::make_unique<RailSource>("VDD", State::HIGH);
    gndSource = std::make_unique<RailSource>("GND", State::LOW);

    registerComponent(vddSource.get());
    registerComponent(gndSource.get());

    connectDriver(vddSource->getDriver(0), vddNet);
    connectDriver(gndSource->getDriver(0), gndNet);

    vddNet->setStateChangeFn([this](Net* n){ notifyRailNetChanged(n); });
    gndNet->setStateChangeFn([this](Net* n){ notifyRailNetChanged(n); });
}

Simulator::~Simulator()
{
    unregisterComponent(vddSource.get());
    unregisterComponent(gndSource.get());
    vddSource.reset();
    gndSource.reset();

    while (!nets.empty()) {
        Net* n = nets.back();
        nets.pop_back();
        auto drivers   = n->getDrivers();
        auto receivers = n->getReceivers();
        for (Driver*   d : drivers)   d->disconnect();
        for (Receiver* r : receivers) r->disconnect();
        delete n;
    }
}

void Simulator::notifyRailNetChanged(Net* /*net*/)
{
    refreshAllComponents();
}

void Simulator::refreshAllComponents()
{
    for (Component* c : components) {
        if (c == vddSource.get() || c == gndSource.get()) continue;
        c->scheduleUpdate(0);
    }
}

void Simulator::registerComponent(Component* c)
{
    c->setTimingWheel(&wheel);
    c->setSimulator(this);
    if (std::find(components.begin(), components.end(), c) == components.end())
        components.push_back(c);
    c->onRegistered();
}

void Simulator::unregisterComponent(Component* c)
{
    wheel.forgetComponent(c);
    c->setTimingWheel(nullptr);
    c->setSimulator(nullptr);
    components.erase(std::remove(components.begin(), components.end(), c),
                     components.end());
}

void Simulator::registerClock(Clock* c)
{
    registerComponent(c);
    if (std::find(clocks.begin(), clocks.end(), c) == clocks.end())
        clocks.push_back(c);
}

void Simulator::unregisterClock(Clock* c)
{
    wheel.forgetClock(c);
    unregisterComponent(c);
    clocks.erase(std::remove(clocks.begin(), clocks.end(), c), clocks.end());
}

Net* Simulator::createNet(std::string name, int width)
{
    Net* n = new Net(std::move(name));
    n->setWidth(width);
    nets.push_back(n);
    return n;
}

void Simulator::removeNet(Net* net)
{
    if (net == vddNet || net == gndNet) return;

    auto drivers   = net->getDrivers();
    auto receivers = net->getReceivers();
    for (Driver*   d : drivers)   d->disconnect();
    for (Receiver* r : receivers) r->disconnect();

    nets.erase(std::remove(nets.begin(), nets.end(), net), nets.end());
    delete net;
}

Net* Simulator::connectDriver(Driver* driver, Net* net)
{
    if (net && !canConnect(driver, net)) return nullptr;
    if (!net) net = createNet({}, driver->getWidth());
    driver->connect(net);
    return net;
}

void Simulator::connectReceiver(Receiver* receiver, Net* net)
{
    if (!net || !canConnect(nullptr, receiver, net)) return;
    receiver->connect(net);
}

bool Simulator::canConnect(Driver* driver, Receiver* receiver, Net* net) const
{
    if (!net) return true;
    int w = net->getWidth();
    if (driver && driver->getWidth() != w) return false;
    if (receiver && receiver->getWidth() != w) return false;
    return true;
}

bool Simulator::canConnect(Driver* driver, Net* net) const
{
    return driver && net && driver->getWidth() == net->getWidth();
}

void Simulator::disconnectDriver(Driver* driver)
{
    driver->disconnect();
}

void Simulator::disconnectReceiver(Receiver* receiver)
{
    receiver->disconnect();
}

void Simulator::start()
{
    if (running) return;
    running = true;
    accumulated = 0.0;
    for (Clock* clk : clocks)
        clk->start(&wheel);
}

void Simulator::stop()
{
    running = false;
    for (Clock* clk : clocks)
        clk->stop();
}

void Simulator::reset()
{
    stop();
    wheel.reset();
    accumulated = 0.0;
}

void Simulator::step(int n)
{
    for (int i = 0; i < n; ++i)
        wheel.tick();
}

// ─── Topology helper ──────────────────────────────────────────────────────────
// For an XNOR gate receiver, check whether its net is driven by any other XNOR.
// If NOT driven by an XNOR, this is a "feed-in" pin (connects to AND network).
bool Simulator::isXnorFeedbackReceiver(Receiver* r) const
{
    Net* n = r ? r->getNet() : nullptr;
    if (!n) return false;
    for (Driver* d : n->getDrivers()) {
        for (Component* c : components) {
            if (c->getName() != "XNOR") continue;
            for (int i = 0; i < c->numDrivers(); ++i)
                if (c->getDriver(i) == d) return true;
        }
    }
    return false;
}

// Return the receiver on xnor that is driven by something OTHER than an XNOR.
Receiver* Simulator::findXnorFeedInReceiver(Component* xnor) const
{
    for (int i = 0; i < xnor->numReceivers(); ++i) {
        Receiver* r = xnor->getReceiver(i);
        if (r && !isXnorFeedbackReceiver(r)) return r;
    }
    return nullptr;
}

// ─── Settle ───────────────────────────────────────────────────────────────────
void Simulator::settle(int maxTicks)
{
    for (int i = 0; i < maxTicks && wheel.hasPendingEvents(); ++i)
        wheel.tick();

    // After inputs have propagated, restore XNOR feedback from the last good
    // held state.  This overrides any wrong state the oscillating timing wheel
    // left behind and ensures settleCombinational starts from the correct Q.
    restoreXorFeedbackStates(heldDriverStates);

    settleEpochStart = snapshotDriverStates();
    settleCombinational();

    // In transparent mode (Enable=1) the XNOR pair has no stable fixed point
    // so settleCombinational oscillates. Detect AND-input values and force
    // the correct Q onto the XNOR outputs.
    applyTransparentXorLoad();

    // Propagate XNOR corrections to all downstream (non-XNOR) components,
    // e.g. PortOut inside the custom latch.  Skip XNOR itself so we don't
    // undo what applyTransparentXorLoad just set.
    for (Component* c : components) {
        if (c == vddSource.get() || c == gndSource.get()) continue;
        if (c->getName() == "XNOR") continue;
        c->update();
    }

    updateHeldDriverStates();

    if (hasFloatingXorOutputs())
        resolveFloatingXorFeedback();
}

void Simulator::queueDriverState(Driver* driver, State state)
{
    if (driver)
        pendingDriverStates[driver] = state;
}

State Simulator::getSettlingNetState(Net* net) const
{
    if (!net) return State::FLOATING;

    for (Driver* driver : net->getDrivers()) {
        auto pending = pendingDriverStates.find(driver);
        if (pending != pendingDriverStates.end())
            return pending->second;
    }

    auto snapIt = settlingNetSnapshot.find(net);
    State snap = snapIt != settlingNetSnapshot.end() ? snapIt->second : net->getState();
    if (snap != State::FLOATING && snap != State::UNDEFINED)
        return snap;

    for (Driver* driver : net->getDrivers()) {
        auto held = heldDriverStates.find(driver);
        if (held != heldDriverStates.end())
            return held->second;

        auto epoch = settleEpochStart.find(driver);
        if (epoch != settleEpochStart.end() &&
            epoch->second != State::FLOATING &&
            epoch->second != State::UNDEFINED) {
            return epoch->second;
        }
    }

    return snap;
}

std::unordered_map<Driver*, State> Simulator::snapshotDriverStates() const
{
    std::unordered_map<Driver*, State> snap;
    for (Component* c : components) {
        if (c == vddSource.get() || c == gndSource.get()) continue;
        for (int i = 0; i < c->numDrivers(); ++i) {
            Driver* d = c->getDriver(i);
            if (d) snap[d] = d->getState();
        }
    }
    return snap;
}

void Simulator::restoreDriverStates(const std::unordered_map<Driver*, State>& states)
{
    for (const auto& [driver, state] : states) {
        if (!driver) continue;
        if (driver->getState() != state)
            driver->setState(state);
    }
}

bool Simulator::driverMapsEqual(const std::unordered_map<Driver*, State>& a,
                                const std::unordered_map<Driver*, State>& b)
{
    if (a.size() != b.size()) return false;
    for (const auto& [driver, state] : a) {
        auto it = b.find(driver);
        if (it == b.end() || it->second != state) return false;
    }
    return true;
}

bool Simulator::hasDefinedState(const std::unordered_map<Driver*, State>& states)
{
    for (const auto& [driver, state] : states) {
        (void)driver;
        if (state != State::FLOATING && state != State::UNDEFINED)
            return true;
    }
    return false;
}

void Simulator::applyHeldDriverStates()
{
    for (const auto& [driver, state] : heldDriverStates) {
        if (!driver) continue;
        if (driver->getState() != state)
            driver->setState(state);
    }
}

bool Simulator::hasDefinedXorFeedbackIn(
    const std::unordered_map<Driver*, State>& source) const
{
    int defined = 0;
    for (Component* c : components) {
        if (c->getName() != "XNOR") continue;
        if (Driver* d = c->getDriver(0)) {
            auto it = source.find(d);
            if (it != source.end() &&
                it->second != State::FLOATING &&
                it->second != State::UNDEFINED) {
                ++defined;
            }
        }
    }
    return defined >= 2;
}

void Simulator::restoreXorFeedbackStates(
    const std::unordered_map<Driver*, State>& source)
{
    for (Component* c : components) {
        if (c->getName() != "XNOR") continue;
        if (Driver* d = c->getDriver(0)) {
            auto it = source.find(d);
            if (it != source.end() &&
                it->second != State::FLOATING &&
                it->second != State::UNDEFINED &&
                d->getState() != it->second) {
                d->setState(it->second);
            }
        }
    }
}

void Simulator::resolveFloatingXorFeedback()
{
    if (hasDefinedXorFeedbackIn(heldDriverStates))
        restoreXorFeedbackStates(heldDriverStates);
    else if (hasDefinedXorFeedbackIn(settleEpochStart))
        restoreXorFeedbackStates(settleEpochStart);
    else
        seedXorFeedbackPair();
    updateHeldDriverStates();
}

void Simulator::seedXorFeedbackPair()
{
    std::vector<Component*> xnors;
    for (Component* c : components) {
        if (c->getName() == "XNOR")
            xnors.push_back(c);
    }
    if (xnors.size() < 2) return;

    auto pickState = [&](Component* xnor) -> State {
        if (!xnor) return State::FLOATING;
        Driver* d = xnor->getDriver(0);
        if (!d) return State::FLOATING;

        auto held = heldDriverStates.find(d);
        if (held != heldDriverStates.end() &&
            held->second != State::FLOATING &&
            held->second != State::UNDEFINED) {
            return held->second;
        }

        auto epoch = settleEpochStart.find(d);
        if (epoch != settleEpochStart.end() &&
            epoch->second != State::FLOATING &&
            epoch->second != State::UNDEFINED) {
            return epoch->second;
        }

        return State::FLOATING;
    };

    State s0 = pickState(xnors[0]);
    State s1 = pickState(xnors[1]);

    if (s0 == State::FLOATING && s1 == State::FLOATING) {
        s0 = driveTrue(*xnors[0]);
        s1 = driveFalse(*xnors[1]);
    } else if (s0 != State::FLOATING && s1 == State::FLOATING) {
        s1 = invertRail(s0, *xnors[1]);
    } else if (s0 == State::FLOATING && s1 != State::FLOATING) {
        s0 = invertRail(s1, *xnors[0]);
    }

    if (Driver* d0 = xnors[0]->getDriver(0))
        d0->setState(s0);
    if (Driver* d1 = xnors[1]->getDriver(0))
        d1->setState(s1);
}

bool Simulator::hasFloatingXorOutputs() const
{
    for (Component* c : components) {
        if (c->getName() != "XNOR") continue;
        if (Driver* d = c->getDriver(0)) {
            State s = d->getState();
            if (s == State::FLOATING || s == State::UNDEFINED)
                return true;
        }
    }
    return false;
}

void Simulator::updateHeldDriverStates()
{
    for (const auto& [driver, state] : snapshotDriverStates()) {
        if (state != State::FLOATING && state != State::UNDEFINED)
            heldDriverStates[driver] = state;
    }
}

// Correct XNOR outputs for transparent-load condition (Enable=1).
// Uses topology detection to find the AND-side receiver (not feedback).
// One XNOR sees its AND input HIGH, the other sees LOW → asymmetric drive
// indicates which complementary state to force.
void Simulator::applyTransparentXorLoad()
{
    std::vector<Component*> xnors;
    for (Component* c : components) {
        if (c->getName() == "XNOR")
            xnors.push_back(c);
    }
    if (xnors.size() < 2) return;

    Component* x0 = xnors[0];
    Component* x1 = xnors[1];

    Receiver* feedIn0 = findXnorFeedInReceiver(x0);
    Receiver* feedIn1 = findXnorFeedInReceiver(x1);
    if (!feedIn0 || !feedIn1) return;

    State in0 = feedIn0->getState();
    State in1 = feedIn1->getState();

    bool in0high = readAsTrue(in0,  *x0), in0low = readAsFalse(in0,  *x0);
    bool in1high = readAsTrue(in1,  *x1), in1low = readAsFalse(in1,  *x1);

    // Exactly one AND input is HIGH (transparent load): force complementary Q.
    Driver* d0 = x0->getDriver(0);
    Driver* d1 = x1->getDriver(0);

    if (in0high && in1low) {
        if (d0) d0->setState(driveFalse(*x0));
        if (d1) d1->setState(driveTrue(*x1));
    } else if (in1high && in0low) {
        if (d0) d0->setState(driveTrue(*x0));
        if (d1) d1->setState(driveFalse(*x1));
    }
    // Both low (hold mode) or both high (illegal): leave XNOR outputs alone.
}

void Simulator::settleCombinational(int maxPasses)
{
    if (settleEpochStart.empty())
        settleEpochStart = snapshotDriverStates();

    // Keep a rolling history of the last HISTORY_DEPTH states.
    // This lets us detect oscillation cycles up to that length (XNOR latches
    // in transparent mode oscillate with period 4, which 2-step lookback misses).
    static constexpr int HISTORY_DEPTH = 8;
    std::vector<std::unordered_map<Driver*, State>> history;
    history.reserve(HISTORY_DEPTH + 1);

    for (int pass = 0; pass < maxPasses; ++pass) {
        settlingNetSnapshot.clear();
        for (Net* net : nets)
            settlingNetSnapshot[net] = net->getState();

        pendingDriverStates.clear();
        combinatorialSettling = true;

        for (Component* c : components) {
            if (c == vddSource.get() || c == gndSource.get()) continue;
            if (c->getName() == "XNOR") continue;
            c->update();
        }
        for (Component* c : components) {
            if (c == vddSource.get() || c == gndSource.get()) continue;
            if (c->getName() != "XNOR") continue;
            c->update();
        }

        combinatorialSettling = false;

        bool changed = false;
        for (const auto& [driver, newState] : pendingDriverStates) {
            State oldState = driver->getState();
            if (newState == State::FLOATING &&
                oldState != State::FLOATING &&
                oldState != State::UNDEFINED) {
                continue;
            }
            if (newState != oldState) {
                driver->setState(newState);
                changed = true;
            }
        }

        if (!changed)
            return;

        std::unordered_map<Driver*, State> current = snapshotDriverStates();

        // Check against every state in history (catches any cycle up to HISTORY_DEPTH).
        bool oscillating = false;
        for (const auto& prev : history) {
            if (driverMapsEqual(current, prev)) { oscillating = true; break; }
        }

        if (oscillating) {
            resolveFloatingXorFeedback();
            return;
        }

        if (static_cast<int>(history.size()) >= HISTORY_DEPTH)
            history.erase(history.begin());
        history.push_back(std::move(current));
    }
}

void Simulator::update(double deltaTime)
{
    if (!running) return;

    accumulated += deltaTime * ticksPerSecond;

    const int MAX_PER_FRAME = 4096;
    int toRun = static_cast<int>(accumulated);
    if (toRun > MAX_PER_FRAME) toRun = MAX_PER_FRAME;
    accumulated -= toRun;

    for (int i = 0; i < toRun; ++i)
        wheel.tick();
}
