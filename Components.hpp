#include "Pin.hpp"
#include <vector>

class Component
{
public:
    virtual void updateState() = 0;
    virtual State getState() = 0;

    void update() {
        State newState = getState();
        if (state != newState) {
            state = newState;
            for (Pin* output : outputs) {
                output->setState(state);
            }
        }
    }

private:
    State state = State::UNDEFINED;
    std::vector<Pin*> inputs;
    std::vector<Pin*> outputs;
};

class Gate : public Component
{
public:
    Gate(const std::vector<Pin*>& inputs, const std::vector<Pin*>& outputs) : Component(inputs, outputs) {}
};