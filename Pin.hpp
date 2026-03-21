#pragma once

#include "Components.hpp"
#include "Net.hpp"

class Pin {
    public:
        Pin(Component* component) : net(nullptr), component(component), state(State::UNDEFINED) {}

        State getState() const {
            return state;
        }

        virtual void setState(State newState) = 0;

    protected:
        Net* net;
        Component* component;
        State state;
};

class Driver : public Pin {
    public:
        Driver(Component* component) : Pin(component) {}

        void setState(State newState) override {
            if (state != newState) {
                net->update(state, newState);
                state = newState;
            }
        }
};

class Receiver : public Pin {
    public:
        Receiver(Component* component) : Pin(component) {}

        void setState(State newState) override{
            state = newState;
            component->update();
        }
};