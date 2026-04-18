#include "Net.hpp"
#include <vector>
#include <map>

class Component
{
public:
    virtual ~Component() = default;
    
    // Called by the component to update the state of its drivers after processing changes in its receivers
    virtual void update(Receiver* updatedReceiver) {
        std::map<Receiver*, int>::iterator it = recieverToPinIndex.find(updatedReceiver);

        if (it != recieverToPinIndex.end()) {
            int driverIndex = it->second;

            if (driverIndex < pinToDriver.size()) {
                Driver* driver = pinToDriver[driverIndex];
                driver->setState(updatedReceiver->getState());
            }
        }
    }

protected:    
    std::map<Receiver*, int> recieverToPinIndex;
    std::vector<Driver*> pinToDriver;
};

class Gate : public Component
{
    virtual void update(Receiver* updatedReceiver) = 0;
};

class AndGate : public Gate
{    
    void update(Receiver* updatedReceiver) override {
        State state1 = pinToDriver[0]->getState();
        State state2 = pinToDriver[1]->getState();

        if (state1 == State::LOW || state2 == State::LOW) {
            pinToDriver[0]->setState(State::LOW);
        } else if (state1 == State::HIGH && state2 == State::HIGH) {
            pinToDriver[0]->setState(State::HIGH);
        } else if (state1 == State::UNDEFINED || state2 == State::UNDEFINED) {
            pinToDriver[0]->setState(State::UNDEFINED);
        } else {
            pinToDriver[0]->setState(State::FLOATING);
        }
    }
};

class OrGate : public Gate
{
    void update(Receiver* updatedReceiver) override {
        State state1 = pinToDriver[0]->getState();
        State state2 = pinToDriver[1]->getState();

        if (state1 == State::HIGH || state2 == State::HIGH) {
            pinToDriver[0]->setState(State::HIGH);
        } else if (state1 == State::LOW && state2 == State::LOW) {
            pinToDriver[0]->setState(State::LOW);
        } else if (state1 == State::UNDEFINED || state2 == State::UNDEFINED) {
            pinToDriver[0]->setState(State::UNDEFINED);
        } else {
            pinToDriver[0]->setState(State::FLOATING);
        }
    }
};

class NotGate : public Gate
{
    void update(Receiver* updatedReceiver) override {
        State state = pinToDriver[0]->getState();

        if (state == State::HIGH) {
            pinToDriver[0]->setState(State::LOW);
        } else if (state == State::LOW) {
            pinToDriver[0]->setState(State::HIGH);
        } else if (state == State::UNDEFINED) {
            pinToDriver[0]->setState(State::UNDEFINED);
        } else {
            pinToDriver[0]->setState(State::FLOATING);
        }
    }
};