#pragma once
#include "State.hpp"

class Simulator;
class Component;

State vddLevel(const Simulator& sim);
State gndLevel(const Simulator& sim);
bool  readAsTrue(State s, const Simulator& sim);
bool  readAsFalse(State s, const Simulator& sim);

State vddLevel(const Component& comp);
State gndLevel(const Component& comp);

State driveTrue(const Component& comp);
State driveFalse(const Component& comp);
bool  readAsTrue(State s, const Component& comp);
bool  readAsFalse(State s, const Component& comp);

// Rail-relative gate helpers
State resolveAndRail(State a, State b, const Component& comp);
State resolveOrRail(State a, State b, const Component& comp);
State resolveXorRail(State a, State b, const Component& comp);
State invertRail(State s, const Component& comp);

// Human-readable label for UI (VDD/GND/FLOAT/UNDEF/OTHER)
const char* stateLabel(State s, const Simulator& sim);

// Rail-relative wire/pin colors (green=VDD level, blue=GND level)
unsigned int stateColorRail(State s, const Simulator& sim);
