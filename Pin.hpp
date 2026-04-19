#pragma once

#include "State.hpp"
#include "Components.hpp"
#include "Net.hpp"

enum class PinType {
    UNDETERMINED,
    DRIVER,
    RECEIVER
};

class Pin {
    public:
        Pin(Component* component) : net(nullptr), component(component), state(State::UNDEFINED) {}

        virtual ~Pin() {
            if (net) {
                net->update(state, State::UNDEFINED);
            }
        };

        State getState() const {
            return state;
        }

        virtual void setState(State newState) = 0;

        Component* getComponent() const {
            return component;
        }

        void connectToNet(Net* newNet) {
            net = newNet;
        }

        PinType getType() const {
            return type;
        }

    protected:
        Net* net;
        Component* component;
        State state;
        PinType type = PinType::UNDETERMINED;
};

class Driver : public Pin {
    public:
        Driver(Component* component) : Pin(component) {
            type = PinType::DRIVER;
        }

        void setState(State newState) override {
            if (state != newState) {
                if (net) {
                    net->update(state, newState);
                }
                state = newState;
            }
        }
};

class Receiver : public Pin {
    public:
        Receiver(Component* component) : Pin(component) {
            type = PinType::RECEIVER;
        }

        void setState(State newState) override {
            if (state != newState) {
                state = newState;
                component->update(this);
            }
        }
};