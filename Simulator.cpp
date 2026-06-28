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

void Simulator::settle(int maxTicks)
{
    for (int i = 0; i < maxTicks && wheel.hasPendingEvents(); ++i)
        wheel.tick();

    for (Component* c : components) {
        if (c == vddSource.get() || c == gndSource.get()) continue;
        c->update();
    }

    settleEpochStart = snapshotDriverStates();
    settleCombinational();
    applyTransparentXorLoad();
    applyHoldXorFeedback();
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

void Simulator::applyTransparentXorLoad()
{
    std::vector<Component*> xnors;
    for (Component* c : components) {
        if (c->getName() == "XNOR")
            xnors.push_back(c);
    }
    if (xnors.size() < 2) return;

    Component* topXnor = xnors[0];
    Component* bottomXnor = xnors[1];
    Receiver* topPath = topXnor->getReceiver(0);
    Receiver* bottomPath = bottomXnor->getReceiver(0);
    if (!topPath || !bottomPath) return;

    State topIn = topPath->getState();
    State bottomIn = bottomPath->getState();

    bool topHigh = readAsTrue(topIn, *topXnor);
    bool topLow = readAsFalse(topIn, *topXnor);
    bool bottomHigh = readAsTrue(bottomIn, *bottomXnor);
    bool bottomLow = readAsFalse(bottomIn, *bottomXnor);

    // Transparent load: one side driven, the other held low by the AND network.
    if (topHigh && bottomLow) {
        if (Driver* d0 = topXnor->getDriver(0))
            d0->setState(driveFalse(*topXnor));
        if (Driver* d1 = bottomXnor->getDriver(0))
            d1->setState(driveTrue(*bottomXnor));
    } else if (topLow && bottomHigh) {
        if (Driver* d0 = topXnor->getDriver(0))
            d0->setState(driveTrue(*topXnor));
        if (Driver* d1 = bottomXnor->getDriver(0))
            d1->setState(driveFalse(*bottomXnor));
    }
}

void Simulator::applyHoldXorFeedback()
{
    std::vector<Component*> xnors;
    for (Component* c : components) {
        if (c->getName() == "XNOR")
            xnors.push_back(c);
    }
    if (xnors.size() < 2) return;

    Component* topXnor = xnors[0];
    Component* bottomXnor = xnors[1];
    Receiver* topPath = topXnor->getReceiver(0);
    Receiver* bottomPath = bottomXnor->getReceiver(0);
    if (!topPath || !bottomPath) return;

    if (!readAsFalse(topPath->getState(), *topXnor) ||
        !readAsFalse(bottomPath->getState(), *bottomXnor)) {
        return;
    }

    if (hasDefinedXorFeedbackIn(heldDriverStates))
        restoreXorFeedbackStates(heldDriverStates);
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

void Simulator::settleCombinational(int maxPasses)
{
    if (settleEpochStart.empty())
        settleEpochStart = snapshotDriverStates();

    std::unordered_map<Driver*, State> prevPass;
    std::unordered_map<Driver*, State> prevPrevPass;

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

        std::unordered_map<Driver*, State> current = snapshotDriverStates();

        if (!changed)
            return;

        if (pass >= 2 && driverMapsEqual(current, prevPrevPass)) {
            resolveFloatingXorFeedback();
            return;
        }

        prevPrevPass = std::move(prevPass);
        prevPass = std::move(current);
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
