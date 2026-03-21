#include "Components.hpp"
#include "Net.hpp"

enum class PinType {
    INPUT,
    OUTPUT
};

class Pin {
    public:
        Pin(Component* component, PinType type) : component(component), type(type) {
            state = component->getState();
        }

        void setState(State newState) {
            if (state != newState) {
                net.update(state, newState);
                state = newState;
                component->update();
            }
        }

    private:
        Net* net;
        Component* component;
        PinType type;
        State state;
};