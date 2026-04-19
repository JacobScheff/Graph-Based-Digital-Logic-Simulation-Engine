#pragma once
#include <vector>

#include "State.hpp"

class Driver;
class Receiver;

class Net
{
public:
    void addDriver(Driver *driver);

    void addReceiver(Receiver *receiver);

    void update(State oldState, State newState);

private:
    void broadcastStateToReceivers();

    void updateState();

    std::vector<Driver *> drivers;
    std::vector<Receiver *> receivers;
    int driverStateCounts[4] = {0, 0, 0, 0}; // LOW, HIGH, FLOATING, UNDEFINED
    State state = State::FLOATING;
};