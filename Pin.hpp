#pragma once

#include "Components.hpp"
#include "Net.hpp"

enum class State
{
    LOW = 0,
    HIGH = 1,
    FLOATING = 2,
    UNDEFINED = 3
};

class Pin {
    public:
        Pin(Component* component) : net(nullptr), component(component), state(State::UNDEFINED) {}

        State getState() const {
            return state;
        }

        virtual void setState(State newState) = 0;

        Component* getComponent() const {
            return component;
        }

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

        void setState(State newState) override {
            if (state != newState) {
                state = newState;
                component->update(this);
            }
        }
};