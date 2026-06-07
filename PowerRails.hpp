#pragma once
#include "State.hpp"

class Simulator;

State vddLevel(const Simulator& sim);
State gndLevel(const Simulator& sim);
State driveTrue(const Simulator& sim);
State driveFalse(const Simulator& sim);
bool  readAsTrue(State s, const Simulator& sim);
bool  readAsFalse(State s, const Simulator& sim);

// Rail-relative gate helpers
State resolveAndRail(State a, State b, const Simulator& sim);
State resolveOrRail(State a, State b, const Simulator& sim);
State resolveXorRail(State a, State b, const Simulator& sim);
State invertRail(State s, const Simulator& sim);

// Human-readable label for UI (VDD/GND/FLOAT/UNDEF/OTHER)
const char* stateLabel(State s, const Simulator& sim);

// Rail-relative wire/pin colors (green=VDD level, blue=GND level)
unsigned int stateColorRail(State s, const Simulator& sim);
