#pragma once
#include <vector>
#include <map>
#include <stdexcept>

#include "Components.hpp"
#include "Net.hpp"
#include "Pin.hpp"

class InputSwitch : public Component {
public:
    InputSwitch() {
        outputs.push_back(new Driver(this));
    }

    void update(Receiver* updatedReceiver) override {
        // Switches don't receive inputs, do nothing.
    }

    void setState(State s) {
        outputs[0]->setState(s);
    }
};

class OutputProbe : public Component {
public:
    OutputProbe() {
        inputs.push_back(new Receiver(this));
    }

    void update(Receiver* updatedReceiver) override {
        // Automatically called when the net state changes
    }

    State getState() {
        return inputs[0]->getState();
    }
};