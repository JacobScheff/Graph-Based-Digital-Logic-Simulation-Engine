#include "Simulator.hpp"
#include "Components.hpp"
#include "IO.hpp"
#include "Pin.hpp"
#include "Net.hpp"
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
