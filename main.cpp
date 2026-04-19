#include <iostream>

#include "Components.hpp"
#include "Net.hpp"
#include "Pin.hpp"
#include "CompositeComponent.hpp"
#include "Gates.hpp"
#include "IO.hpp"

// Helper function to print states neatly
std::string stateToString(State state) {
    switch (state) {
        case State::LOW: return "LOW  (0)";
        case State::HIGH: return "HIGH (1)";
        case State::FLOATING: return "FLOAT(Z)";
        case State::UNDEFINED: return "UNDEF(X)";
        default: return "UNKNOWN";
    }
}

int main() {
    std::cout << "--- Logic Circuit Simulator Test ---\n";
    std::cout << "Building a Half Adder (A, B -> Sum, Carry)\n\n";

    // 1. Instantiate Components
    InputSwitch switchA;
    InputSwitch switchB;
    
    XorGate xorGate; // Calculates Sum
    AndGate andGate; // Calculates Carry
    
    OutputProbe sumProbe;
    OutputProbe carryProbe;

    // 2. Instantiate Nets (Wires)
    Net netA;
    Net netB;
    Net netSum;
    Net netCarry;

    // 3. Wire the circuit together

    // Connect Input A to Net A, and route Net A to XOR and AND gates
    netA.addDriver(switchA.getOutput(0));
    switchA.getOutput(0)->connectToNet(&netA); // Crucial: Driver needs to know its net
    
    netA.addReceiver(xorGate.getInput(0));
    netA.addReceiver(andGate.getInput(0));

    // Connect Input B to Net B, and route Net B to XOR and AND gates
    netB.addDriver(switchB.getOutput(0));
    switchB.getOutput(0)->connectToNet(&netB);
    
    netB.addReceiver(xorGate.getInput(1));
    netB.addReceiver(andGate.getInput(1));

    // Connect XOR output to Net Sum, and route to Sum Probe
    netSum.addDriver(xorGate.getOutput(0));
    xorGate.getOutput(0)->connectToNet(&netSum);
    
    netSum.addReceiver(sumProbe.getInput(0));

    // Connect AND output to Net Carry, and route to Carry Probe
    netCarry.addDriver(andGate.getOutput(0));
    andGate.getOutput(0)->connectToNet(&netCarry);
    
    netCarry.addReceiver(carryProbe.getInput(0));

    // 4. Test the Truth Table
    std::vector<State> testStates = {State::LOW, State::HIGH};

    std::cout << "A        | B        | SUM      | CARRY\n";
    std::cout << "------------------------------------------\n";

    for (State stateA : testStates) {
        for (State stateB : testStates) {
            
            // Apply inputs. Because your system is event-driven via call stacks,
            // setting the state here will instantly ripple through Nets -> Receivers -> 
            // Gates -> Drivers -> Nets -> Probes.
            switchA.setState(stateA);
            switchB.setState(stateB);

            // Read Outputs
            State sumResult = sumProbe.getState();
            State carryResult = carryProbe.getState();

            // Print the row
            std::cout << stateToString(stateA) << " | "
                      << stateToString(stateB) << " | "
                      << stateToString(sumResult) << " | "
                      << stateToString(carryResult) << "\n";
        }
    }

    std::cout << "\nTest Complete.\n";
    return 0;
}