#include "Net.hpp"
#include "Pin.hpp"
#include <algorithm>

Net::Net(std::string name) : name(std::move(name)) {}

// ─── Driver registration ──────────────────────────────────────────────────────

void Net::addDriver(Driver* d)
{
    drivers.push_back(d);
    counts[static_cast<int>(d->getState())]++;
    recomputeState();
}

void Net::removeDriver(Driver* d)
{
    counts[static_cast<int>(d->getState())]--;
    drivers.erase(std::remove(drivers.begin(), drivers.end(), d), drivers.end());
    recomputeState();
}

// ─── Receiver registration ────────────────────────────────────────────────────

void Net::addReceiver(Receiver* r)
{
    receivers.push_back(r);
}

void Net::removeReceiver(Receiver* r)
{
    receivers.erase(std::remove(receivers.begin(), receivers.end(), r), receivers.end());
}

// ─── State update ─────────────────────────────────────────────────────────────

void Net::onDriverChanged(State oldState, State newState)
{
    counts[static_cast<int>(oldState)]--;
    counts[static_cast<int>(newState)]++;
    recomputeState();
}

void Net::recomputeState()
{
    State prev = state;

    // Priority: UNDEFINED > contention(H+L) > HIGH > LOW > FLOATING
    if (counts[static_cast<int>(State::UNDEFINED)] > 0) {
        state = State::UNDEFINED;
    } else if (counts[static_cast<int>(State::HIGH)] > 0 &&
               counts[static_cast<int>(State::LOW)]  > 0) {
        state = State::UNDEFINED;   // bus contention
    } else if (counts[static_cast<int>(State::HIGH)] > 0) {
        state = State::HIGH;
    } else if (counts[static_cast<int>(State::LOW)] > 0) {
        state = State::LOW;
    } else {
        state = State::FLOATING;
    }

    if (state != prev) {
        for (Receiver* r : receivers)
            r->onNetChanged(prev, state);
        if (onStateChange)
            onStateChange(this);
    }
}
