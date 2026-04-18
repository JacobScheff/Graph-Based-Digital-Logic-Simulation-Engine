#include <vector>
#include "Pin.hpp"

class Net
{
public:
    void addDriver(Driver *driver)
    {
        drivers.push_back(driver);
        driverStateCounts[static_cast<int>(driver->getState())]++;
        updateState();
    }

    void addReceiver(Receiver *receiver)
    {
        receivers.push_back(receiver);
    }

    void update(State oldState, State newState)
    {
        if (oldState != newState)
        {
            driverStateCounts[static_cast<int>(oldState)]--;
            driverStateCounts[static_cast<int>(newState)]++;
            updateState();
        }
    }

private:
    void broadcastStateToReceivers()
    {
        for (Receiver *receiver : receivers)
        {
            receiver->setState(state);
        }
    }

    void updateState()
    {
        State prevState = state;

        // FLOATING pins do not affect the state, so start with FLOATING as the default state
        state = State::FLOATING;

        // If there are any UNDEFINED drivers, the state is UNDEFINED
        if (driverStateCounts[static_cast<int>(State::UNDEFINED)] > 0)
        {
            state = State::UNDEFINED; // X + UNDEFINED = UNDEFINED
            return;
        }

        // Check LOW states
        if (driverStateCounts[static_cast<int>(State::LOW)] > 0)
        {
            state = State::LOW; // LOW + FLOATING = LOW
        }

        // Check HIGH states
        if (driverStateCounts[static_cast<int>(State::HIGH)] > 0)
        {
            if (state == State::LOW)
            {
                state = State::UNDEFINED; // LOW + HIGH = UNDEFINED
            }
            else
            {
                state = State::HIGH; // FLOATING + HIGH = HIGH
            }
        }

        // If the state changed, broadcast the new state to all receivers
        if (state != prevState)
        {
            broadcastStateToReceivers();
        }
    }

    std::vector<Driver *> drivers;
    std::vector<Receiver *> receivers;
    int driverStateCounts[4] = {0, 0, 0, 0}; // LOW, HIGH, FLOATING, UNDEFINED
    State state = State::FLOATING;
};