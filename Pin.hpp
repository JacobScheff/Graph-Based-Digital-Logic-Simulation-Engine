#pragma once
#include "State.hpp"

class Net;
class Component;

// ─── Base ─────────────────────────────────────────────────────────────────────

class Pin
{
public:
    virtual ~Pin() = default;
    Net*  getNet()       const { return net; }
    bool  isConnected()  const { return net != nullptr; }
    virtual State getState() const = 0;

    int   getWidth()     const { return width; }
    void  setWidth(int w)      { if (w >= 1) width = w; }

protected:
    Net* net = nullptr;
    int  width = 1;
};

// ─── Driver ───────────────────────────────────────────────────────────────────
// An output pin.  Drives a net with a specific logic state.

class Driver : public Pin
{
public:
    explicit Driver(Component* owner);
    ~Driver() override;

    void  connect(Net* net);
    void  disconnect();

    // Update the driven state and propagate through the net
    void  setState(State newState);
    State getState() const override { return state; }

private:
    Component* owner;
    State      state = State::FLOATING;
};

// ─── Receiver ─────────────────────────────────────────────────────────────────
// An input pin.  Reads the net state and tells the owner component to
// reschedule itself on the timing wheel when the net changes.

class Receiver : public Pin
{
public:
    Receiver(Component* owner, int propagationDelay = 1);
    ~Receiver() override;

    void  connect(Net* net);
    void  disconnect();

    State getState() const override;

    // Called by Net::recomputeState when the net's resolved state changes
    void onNetChanged(State oldState, State newState);

    int getPropagationDelay() const { return propagationDelay; }

private:
    Component* owner;
    int        propagationDelay;
};
