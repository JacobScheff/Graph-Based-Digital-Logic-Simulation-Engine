#pragma once
#include <vector>
#include <string>
#include <functional>
#include "State.hpp"

class Driver;
class Receiver;

// A Net is a single electrical node.  It collects the states of all
// driving pins, resolves the wired logic, and notifies receivers.
class Net
{
public:
    using StateChangeFn = std::function<void(Net*)>;

    explicit Net(std::string name = "");
    ~Net() = default;

    void addDriver(Driver* d);
    void removeDriver(Driver* d);

    void addReceiver(Receiver* r);
    void removeReceiver(Receiver* r);

    void onDriverChanged(State oldState, State newState);

    State getState() const { return state; }

    int   getWidth() const { return width; }
    void  setWidth(int w) { if (w >= 1) width = w; }

    const std::string& getName() const { return name; }
    void setName(const std::string& n) { name = n; }

    void setStateChangeFn(StateChangeFn fn) { onStateChange = std::move(fn); }

    const std::vector<Driver*>&   getDrivers()   const { return drivers;   }
    const std::vector<Receiver*>& getReceivers() const { return receivers; }

private:
    void recomputeState();

    std::string             name;
    std::vector<Driver*>   drivers;
    std::vector<Receiver*> receivers;
    StateChangeFn           onStateChange;

    int counts[4] = {0, 0, 0, 0};

    State state = State::FLOATING;
    int   width = 1;
};
