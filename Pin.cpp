#include "Pin.hpp"
#include "Net.hpp"
#include "Components.hpp"

// ════════════════════════════════ Driver ═════════════════════════════════════

Driver::Driver(Component* owner) : owner(owner) {}

Driver::~Driver()
{
    disconnect();
}

void Driver::connect(Net* net)
{
    disconnect();          // detach from any previous net
    this->net = net;
    net->addDriver(this);  // net counts our current state immediately
    owner->scheduleUpdate(1);
}

void Driver::disconnect()
{
    if (net) {
        net->removeDriver(this);
        net = nullptr;
        owner->scheduleUpdate(1);
    }
}

void Driver::setState(State newState)
{
    if (newState == state) return;
    State old = state;
    state = newState;
    if (net)
        net->onDriverChanged(old, newState);
}

// ════════════════════════════════ Receiver ════════════════════════════════════

Receiver::Receiver(Component* owner, int propagationDelay)
    : owner(owner), propagationDelay(propagationDelay) {}

Receiver::~Receiver()
{
    disconnect();
}

void Receiver::connect(Net* net)
{
    disconnect();
    this->net = net;
    net->addReceiver(this);
    owner->scheduleUpdate(propagationDelay);
}

void Receiver::disconnect()
{
    if (net) {
        net->removeReceiver(this);
        net = nullptr;
        owner->scheduleUpdate(propagationDelay);
    }
}

State Receiver::getState() const
{
    return net ? net->getState() : State::FLOATING;
}

void Receiver::onNetChanged(State /*oldState*/, State /*newState*/)
{
    owner->scheduleUpdate(propagationDelay);
}
