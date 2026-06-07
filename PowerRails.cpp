#include "PowerRails.hpp"
#include "Simulator.hpp"
#include "Net.hpp"

State vddLevel(const Simulator& sim) { return sim.getVddNet()->getState(); }
State gndLevel(const Simulator& sim) { return sim.getGndNet()->getState(); }

State driveTrue(const Simulator& sim)  { return vddLevel(sim); }
State driveFalse(const Simulator& sim) { return gndLevel(sim); }

bool readAsTrue(State s, const Simulator& sim)  { return s == vddLevel(sim); }
bool readAsFalse(State s, const Simulator& sim) { return s == gndLevel(sim); }

State resolveAndRail(State a, State b, const Simulator& sim)
{
    bool a1 = readAsTrue(a, sim), a0 = readAsFalse(a, sim);
    bool b1 = readAsTrue(b, sim), b0 = readAsFalse(b, sim);
    if (a0 || b0) return driveFalse(sim);
    if (a1 && b1) return driveTrue(sim);
    if (a == State::UNDEFINED || b == State::UNDEFINED) return State::UNDEFINED;
    return State::FLOATING;
}

State resolveOrRail(State a, State b, const Simulator& sim)
{
    bool a1 = readAsTrue(a, sim), a0 = readAsFalse(a, sim);
    bool b1 = readAsTrue(b, sim), b0 = readAsFalse(b, sim);
    if (a1 || b1) return driveTrue(sim);
    if (a0 && b0) return driveFalse(sim);
    if (a == State::UNDEFINED || b == State::UNDEFINED) return State::UNDEFINED;
    return State::FLOATING;
}

State resolveXorRail(State a, State b, const Simulator& sim)
{
    if (a == State::UNDEFINED || b == State::UNDEFINED) return State::UNDEFINED;
    if (a == State::FLOATING  || b == State::FLOATING)  return State::FLOATING;
    bool a1 = readAsTrue(a, sim);
    bool b1 = readAsTrue(b, sim);
    return (a1 != b1) ? driveTrue(sim) : driveFalse(sim);
}

State invertRail(State s, const Simulator& sim)
{
    if (readAsTrue(s, sim))  return driveFalse(sim);
    if (readAsFalse(s, sim)) return driveTrue(sim);
    if (s == State::FLOATING)  return State::UNDEFINED;
    return State::UNDEFINED;
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
    if (readAsTrue(s, sim))  return 0xFF4CAF50; // green  – VDD level
    if (readAsFalse(s, sim)) return 0xFF2196F3; // blue   – GND level
    switch (s) {
        case State::FLOATING:  return 0xFF787878;
        case State::UNDEFINED: return 0xFFF44336;
        default:               return 0xFFFFFFFF;
    }
}
