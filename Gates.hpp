#pragma once
#include "Components.hpp"
#include "PowerRails.hpp"

// ─── Helper: resolve AND-like truth table (legacy, unused) ────────────────────
static inline State resolveAnd(State a, State b)
{
    if (a == State::LOW  || b == State::LOW)  return State::LOW;
    if (a == State::HIGH && b == State::HIGH) return State::HIGH;
    if (a == State::UNDEFINED || b == State::UNDEFINED) return State::UNDEFINED;
    return State::FLOATING;
}

static inline State resolveOr(State a, State b)
{
    if (a == State::HIGH || b == State::HIGH) return State::HIGH;
    if (a == State::LOW  && b == State::LOW)  return State::LOW;
    if (a == State::UNDEFINED || b == State::UNDEFINED) return State::UNDEFINED;
    return State::FLOATING;
}

static inline State resolveXor(State a, State b)
{
    if (a == State::UNDEFINED || b == State::UNDEFINED) return State::UNDEFINED;
    if (a == State::FLOATING  || b == State::FLOATING)  return State::FLOATING;
    return (a != b) ? State::HIGH : State::LOW;
}

static inline State invert(State s)
{
    switch (s) {
        case State::HIGH:      return State::LOW;
        case State::LOW:       return State::HIGH;
        case State::FLOATING:  return State::UNDEFINED;
        case State::UNDEFINED: return State::UNDEFINED;
    }
    return State::UNDEFINED;
}

// ─── NOT Gate ─────────────────────────────────────────────────────────────────
class NotGate : public Component
{
public:
    NotGate() : Component("NOT", 3, 1) {}
    void update() override {
        if (!getSimulator()) return;
        drivers[0]->setState(invertRail(receivers[0]->getState(), *this));
    }
};

// ─── Buffer ───────────────────────────────────────────────────────────────────
class Buffer : public Component
{
public:
    Buffer() : Component("BUF", 3, 1) {}
    void update() override {
        if (!getSimulator()) return;
        State s = receivers[0]->getState();
        if (readAsTrue(s, *this))       drivers[0]->setState(driveTrue(*this));
        else if (readAsFalse(s, *this)) drivers[0]->setState(driveFalse(*this));
        else                            drivers[0]->setState(State::UNDEFINED);
    }
};

// ─── AND Gate ─────────────────────────────────────────────────────────────────
class AndGate : public Component
{
public:
    AndGate() : Component("AND", 4, 1) {}
    void update() override {
        if (!getSimulator()) return;
        drivers[0]->setState(resolveAndRail(receivers[0]->getState(),
                                            receivers[1]->getState(), *this));
    }
};

// ─── NAND Gate ────────────────────────────────────────────────────────────────
class NandGate : public Component
{
public:
    NandGate() : Component("NAND", 4, 1) {}
    void update() override {
        if (!getSimulator()) return;
        drivers[0]->setState(invertRail(
            resolveAndRail(receivers[0]->getState(),
                           receivers[1]->getState(), *this), *this));
    }
};

// ─── OR Gate ──────────────────────────────────────────────────────────────────
class OrGate : public Component
{
public:
    OrGate() : Component("OR", 4, 1) {}
    void update() override {
        if (!getSimulator()) return;
        drivers[0]->setState(resolveOrRail(receivers[0]->getState(),
                                           receivers[1]->getState(), *this));
    }
};

// ─── NOR Gate ─────────────────────────────────────────────────────────────────
class NorGate : public Component
{
public:
    NorGate() : Component("NOR", 4, 1) {}
    void update() override {
        if (!getSimulator()) return;
        drivers[0]->setState(invertRail(
            resolveOrRail(receivers[0]->getState(),
                          receivers[1]->getState(), *this), *this));
    }
};

// ─── XOR Gate ─────────────────────────────────────────────────────────────────
class XorGate : public Component
{
public:
    XorGate() : Component("XOR", 4, 1) {}
    void update() override {
        if (!getSimulator()) return;
        drivers[0]->setState(resolveXorRail(receivers[0]->getState(),
                                            receivers[1]->getState(), *this));
    }
};

// ─── XNOR Gate ────────────────────────────────────────────────────────────────
class XnorGate : public Component
{
public:
    XnorGate() : Component("XNOR", 4, 1) {}
    void update() override {
        if (!getSimulator()) return;
        drivers[0]->setState(invertRail(
            resolveXorRail(receivers[0]->getState(),
                           receivers[1]->getState(), *this), *this));
    }
};
