#pragma once
#include "Pin.hpp"

#include <vector>
#include <map>

class NotGate : public Component
{    
public:
    NotGate() {
        inputs.push_back(new Receiver(this));
        outputs.push_back(new Driver(this));
    }

    void update(Receiver* updatedReceiver) override {
        State state = inputs[0]->getState();

        State result;

        if (state == State::HIGH) {
            result = State::LOW;
        } else if (state == State::LOW) {
            result = State::HIGH;
        } else if (state == State::UNDEFINED) {
            result = State::UNDEFINED;
        } else {
            result = State::FLOATING;
        }

        outputs[0]->setState(result);
    }
};

class AndGate : public Component
{
public:
    AndGate() {
        inputs.push_back(new Receiver(this));
        inputs.push_back(new Receiver(this));
        outputs.push_back(new Driver(this));
    }

    void update(Receiver* updatedReceiver) override {
        State state1 = inputs[0]->getState();
        State state2 = inputs[1]->getState();

        State result;

        if (state1 == State::LOW || state2 == State::LOW) {
            result = State::LOW;
        } else if (state1 == State::HIGH && state2 == State::HIGH) {
            result = State::HIGH;
        } else if (state1 == State::UNDEFINED || state2 == State::UNDEFINED) {
            result = State::UNDEFINED;
        } else {
            result = State::FLOATING;
        }

        outputs[0]->setState(result);
    }
};

class NandGate : public Component
{
public:
    NandGate() {
        inputs.push_back(new Receiver(this));
        inputs.push_back(new Receiver(this));
        outputs.push_back(new Driver(this));
    }

    void update(Receiver* updatedReceiver) override {
        State state1 = inputs[0]->getState();
        State state2 = inputs[1]->getState();

        State result;

        if (state1 == State::LOW || state2 == State::LOW) {
            result = State::HIGH;
        } else if (state1 == State::HIGH && state2 == State::HIGH) {
            result = State::LOW;
        } else if (state1 == State::UNDEFINED || state2 == State::UNDEFINED) {
            result = State::UNDEFINED;
        } else {
            result = State::FLOATING;
        }

        outputs[0]->setState(result);
    }
};

class OrGate : public Component
{    
public:
    OrGate() {
        inputs.push_back(new Receiver(this));
        inputs.push_back(new Receiver(this));
        outputs.push_back(new Driver(this));
    }

    void update(Receiver* updatedReceiver) override {
        State state1 = inputs[0]->getState();
        State state2 = inputs[1]->getState();

        State result;

        if (state1 == State::HIGH || state2 == State::HIGH) {
            result = State::HIGH;
        } else if (state1 == State::LOW && state2 == State::LOW) {
            result = State::LOW;
        } else if (state1 == State::UNDEFINED || state2 == State::UNDEFINED) {
            result = State::UNDEFINED;
        } else {
            result = State::FLOATING;
        }

        outputs[0]->setState(result);
    }
};

class NorGate : public Component
{    
public:
    NorGate() {
        inputs.push_back(new Receiver(this));
        inputs.push_back(new Receiver(this));
        outputs.push_back(new Driver(this));
    }

    void update(Receiver* updatedReceiver) override {
        State state1 = inputs[0]->getState();
        State state2 = inputs[1]->getState();

        State result;

        if (state1 == State::HIGH || state2 == State::HIGH) {
            result = State::LOW;
        } else if (state1 == State::LOW && state2 == State::LOW) {
            result = State::HIGH;
        } else if (state1 == State::UNDEFINED || state2 == State::UNDEFINED) {
            result = State::UNDEFINED;
        } else {
            result = State::FLOATING;
        }

        outputs[0]->setState(result);
     }
};

class XorGate : public Component
{    
public:
    XorGate() {
        inputs.push_back(new Receiver(this));
        inputs.push_back(new Receiver(this));
        outputs.push_back(new Driver(this));
    }

    void update(Receiver* updatedReceiver) override {
        State state1 = inputs[0]->getState();
        State state2 = inputs[1]->getState();

        State result;

        if (state1 == State::UNDEFINED || state2 == State::UNDEFINED) {
            result = State::UNDEFINED;
        } else if (state1 == State::FLOATING || state2 == State::FLOATING) {
            result = State::FLOATING;
        } else if (state1 != state2) {
            result = State::HIGH;
        } else {
            result = State::LOW;
        }

        outputs[0]->setState(result);
    }
};

class XnorGate : public Component
{  
public:  
    XnorGate() {
        inputs.push_back(new Receiver(this));
        inputs.push_back(new Receiver(this));
        outputs.push_back(new Driver(this));
    }

    void update(Receiver* updatedReceiver) override {
        State state1 = inputs[0]->getState();
        State state2 = inputs[1]->getState();

        State result;

        if (state1 == State::UNDEFINED || state2 == State::UNDEFINED) {
            result = State::UNDEFINED;
        } else if (state1 == State::FLOATING || state2 == State::FLOATING) {
            result = State::FLOATING;
        } else if (state1 == state2) {
            result = State::HIGH;
        } else {
            result = State::LOW;
        }

        outputs[0]->setState(result);
     }
};