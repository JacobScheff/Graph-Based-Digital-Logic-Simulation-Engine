#include "PowerRails.hpp"
#include "Simulator.hpp"
#include "Components.hpp"
#include "Net.hpp"

State vddLevel(const Simulator& sim) { return sim.getVddNet()->getState(); }
State gndLevel(const Simulator& sim) { return sim.getGndNet()->getState(); }
bool  readAsTrue(State s, const Simulator& sim)  { return s == vddLevel(sim); }
bool  readAsFalse(State s, const Simulator& sim) { return s == gndLevel(sim); }

static bool componentHasPowerPins(const Component& comp)
{
    const std::string& type = comp.getName();
    if (type == "SW" || type == "BTN" || type == "LED" || 
        type == "NUM_IN" || type == "NUM_DISP" || type == "RGB_DISP" ||
        type == "JUNCTION" || type == "BUS_MERGE" || type == "BUS_SPLIT" ||
        type == "REG")
        return false;
    return true;
}

State vddLevel(const Component& comp)
{
    if (componentHasPowerPins(comp)) {
        int n = comp.numReceivers();
        if (n >= 2) {
            auto* pin = const_cast<Component&>(comp).getReceiver(n - 2);
            if (pin && pin->isConnected()) return pin->getState();
        }
    }
    return comp.getSimulator() ? vddLevel(*comp.getSimulator()) : State::HIGH;
}

State gndLevel(const Component& comp)
{
    if (componentHasPowerPins(comp)) {
        int n = comp.numReceivers();
        if (n >= 1) {
            auto* pin = const_cast<Component&>(comp).getReceiver(n - 1);
            if (pin && pin->isConnected()) return pin->getState();
        }
    }
    return comp.getSimulator() ? gndLevel(*comp.getSimulator()) : State::LOW;
}

State driveTrue(const Component& comp)  { return vddLevel(comp); }
State driveFalse(const Component& comp) { return gndLevel(comp); }

bool readAsTrue(State s, const Component& comp)  { return s == vddLevel(comp); }
bool readAsFalse(State s, const Component& comp) { return s == gndLevel(comp); }

State resolveAndRail(State a, State b, const Component& comp)
{
    bool a1 = readAsTrue(a, comp), a0 = readAsFalse(a, comp);
    bool b1 = readAsTrue(b, comp), b0 = readAsFalse(b, comp);
    if (a0 || b0) return driveFalse(comp);
    if (a1 && b1) return driveTrue(comp);
    if (a == State::UNDEFINED || b == State::UNDEFINED) return State::UNDEFINED;
    return State::FLOATING;
}

State resolveOrRail(State a, State b, const Component& comp)
{
    bool a1 = readAsTrue(a, comp), a0 = readAsFalse(a, comp);
    bool b1 = readAsTrue(b, comp), b0 = readAsFalse(b, comp);
    if (a1 || b1) return driveTrue(comp);
    if (a0 && b0) return driveFalse(comp);
    if (a == State::UNDEFINED || b == State::UNDEFINED) return State::UNDEFINED;
    return State::FLOATING;
}

State resolveXorRail(State a, State b, const Component& comp)
{
    bool a1 = readAsTrue(a, comp), a0 = readAsFalse(a, comp);
    bool b1 = readAsTrue(b, comp), b0 = readAsFalse(b, comp);
    if ((a1 || a0) && (b1 || b0)) {
        return (a1 != b1) ? driveTrue(comp) : driveFalse(comp);
    }
    if (a == State::UNDEFINED || b == State::UNDEFINED) return State::UNDEFINED;
    return State::FLOATING;
}

State invertRail(State s, const Component& comp)
{
    if (readAsTrue(s, comp))  return driveFalse(comp);
    if (readAsFalse(s, comp)) return driveTrue(comp);
    if (s == State::UNDEFINED) return State::UNDEFINED;
    return State::FLOATING;
}

const char* stateLabel(State s, const Simulator& sim)
{
    if (readAsTrue(s, sim))  return "VDD";
    if (readAsFalse(s, sim)) return "GND";
    switch (s) {
        case State::FLOATING:  return "FLOAT";
        case State::UNDEFINED: return "UNDEF";
        default:               return "OTHER";
    }
}

unsigned int stateColorRail(State s, const Simulator& sim)
{
    if (readAsTrue(s, sim))  return 0xFF50AF4C; // green  – VDD level
    if (readAsFalse(s, sim)) return 0xFFF39621; // blue   – GND level
    switch (s) {
        case State::FLOATING:  return 0xFF787878;
        case State::UNDEFINED: return 0xFF3643F4;
        default:               return 0xFFFFFFFF;
    }
}
