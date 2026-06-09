#pragma once
#include "Components.hpp"
#include "PowerRails.hpp"
#include <cstdint>

class TimingWheel;

// ─── Internal Rail Source (owned by Simulator) ────────────────────────────────
class RailSource : public Component
{
public:
    RailSource(std::string name, State initial)
        : Component(std::move(name), 0, 1)
    {
        drivers[0]->setState(initial);
    }
    void update() override {}
};

// ─── Toggle Switch ────────────────────────────────────────────────────────────
class Switch : public Component
{
public:
    Switch() : Component("SW", 0, 1)
    {
        setOutput(false);
    }

    void toggle()
    {
        setOutput(!outputOn);
    }

    void setOutput(bool on)
    {
        outputOn = on;
        update();
    }

    bool  isOn() const { return outputOn; }
    State getOutputState() const { return drivers[0]->getState(); }

    void onRegistered() override { setOutput(outputOn); }

    void update() override {
        drivers[0]->setState(outputOn ? driveTrue(*this) : driveFalse(*this));
    }

private:
    bool outputOn = false;
};

// ─── Momentary Button ─────────────────────────────────────────────────────────
class Button : public Component
{
public:
    Button() : Component("BTN", 0, 1)
    {
        release();
    }

    void press()
    {
        pressed = true;
        update();
    }
    void release()
    {
        pressed = false;
        update();
    }

    void onRegistered() override
    {
        if (pressed) press();
        else         release();
    }

    bool isPressed() const { return pressed; }
    void update() override {
        drivers[0]->setState(pressed ? driveTrue(*this) : driveFalse(*this));
    }

private:
    bool pressed = false;
};

// ─── Clock ────────────────────────────────────────────────────────────────────
class Clock : public Component
{
public:
    explicit Clock(int halfPeriodTicks = 10)
        : Component("CLK", 2, 1), halfPeriod(halfPeriodTicks)
    {
        drivers[0]->setState(State::LOW);
    }

    void start(TimingWheel* w);
    void stop() { running = false; }
    bool isRunning() const { return running; }
    void onTick(TimingWheel* w);

    void setHalfPeriod(int ticks)
    {
        if (ticks < 1) ticks = 1;
        halfPeriod = ticks;
    }
    int  getHalfPeriod() const { return halfPeriod; }

    void update() override {
        drivers[0]->setState(outputOn ? driveTrue(*this) : driveFalse(*this));
    }

private:
    int   halfPeriod;
    bool  outputOn = false;
    bool  running  = false;
};

// ─── LED ──────────────────────────────────────────────────────────────────────
class LED : public Component
{
public:
    explicit LED(const std::string& label = "LED")
        : Component(label, 1, 0) {}

    State getLitState() const { return receivers[0]->getState(); }
    bool  isLit() const
    {
        if (!getSimulator()) return false;
        return readAsTrue(getLitState(), *this);
    }

    void update() override {}
};

// ─── 4-bit Numeric Input ──────────────────────────────────────────────────────
class NumericInput : public Component
{
public:
    NumericInput() : Component("NUM_IN", 0, 4)
    {
        setBusWidth(4);
        setValue(0);
    }

    void setValue(int v)
    {
        value = v & 0xF;
        if (!getSimulator()) {
            for (int i = 0; i < 4; ++i)
                drivers[i]->setState(((value >> (3 - i)) & 1) ? State::HIGH : State::LOW);
            return;
        }
        for (int i = 0; i < 4; ++i) {
            bool bit = (value >> (3 - i)) & 1;
            drivers[i]->setState(bit ? driveTrue(*this) : driveFalse(*this));
        }
    }

    int  getValue() const { return value; }

    void onRegistered() override { setValue(value); }

    void update() override {}

private:
    int value = 0;
};

// ─── 4-bit Numeric Display ────────────────────────────────────────────────────
class NumericDisplay : public Component
{
public:
    NumericDisplay() : Component("NUM_DISP", 4, 0)
    {
        setBusWidth(4);
    }

    int getValue() const
    {
        if (!getSimulator()) return 0;
        int v = 0;
        for (int i = 0; i < 4; ++i)
            if (readAsTrue(receivers[i]->getState(), *this))
                v |= (8 >> i);
        return v;
    }

    bool hasAmbiguity() const
    {
        if (!getSimulator()) return true;
        for (int i = 0; i < 4; ++i) {
            State s = receivers[i]->getState();
            if (!readAsTrue(s, *this) && !readAsFalse(s, *this))
                return true;
        }
        return false;
    }

    void update() override {}
};

// ─── Wire Junction (pass-through) ───────────────────────────────────────────
class Junction : public Component
{
public:
    Junction() : Component("JUNCTION", 1, 1) {}
    void update() override {
        drivers[0]->setState(receivers[0]->getState());
    }
};

// ─── Bus Merge: N x 1-bit inputs → N-bit bus output ─────────────────────────
class BusMerge : public Component
{
public:
    explicit BusMerge(int width = 4)
        : Component("BUS_MERGE", width, width, 1)
    {
        setBusWidth(width);
    }

    void update() override {
        if (!getSimulator()) return;
        int w = getBusWidth();
        for (int i = 0; i < w; ++i) {
            State s = receivers[i]->getState();
            if (readAsTrue(s, *this))       drivers[i]->setState(driveTrue(*this));
            else if (readAsFalse(s, *this)) drivers[i]->setState(driveFalse(*this));
            else                            drivers[i]->setState(s);
        }
    }
};

// ─── Bus Split: N-bit bus input → N x 1-bit outputs ─────────────────────────
class BusSplit : public Component
{
public:
    explicit BusSplit(int width = 4)
        : Component("BUS_SPLIT", width, width, 1)
    {
        setBusWidth(width);
    }

    void update() override {
        if (!getSimulator()) return;
        int w = getBusWidth();
        for (int i = 0; i < w; ++i) {
            State s = receivers[i]->getState();
            if (readAsTrue(s, *this))       drivers[i]->setState(driveTrue(*this));
            else if (readAsFalse(s, *this)) drivers[i]->setState(driveFalse(*this));
            else                            drivers[i]->setState(s);
        }
    }
};

// ─── Register: N-bit D flip-flop ──────────────────────────────────────────────
class Register : public Component
{
public:
    explicit Register(int width = 4)
        : Component("REG", width + 1, width, 1)
    {
        setBusWidth(width);
        state.resize(width, State::LOW);
    }

    void update() override {
        if (!getSimulator()) return;
        
        State clkState = receivers[getBusWidth()]->getState();
        bool clk = readAsTrue(clkState, *this);
        
        if (clk && !lastClk) {
            // Rising edge: latch inputs
            for (int i = 0; i < getBusWidth(); ++i) {
                State s = receivers[i]->getState();
                state[i] = readAsTrue(s, *this) ? driveTrue(*this) :
                           readAsFalse(s, *this) ? driveFalse(*this) : s;
            }
        }
        lastClk = clk;
        
        // Output latched state
        for (int i = 0; i < getBusWidth(); ++i) {
            drivers[i]->setState(state[i]);
        }
    }

private:
    std::vector<State> state;
    bool lastClk = false;
};
// ─── Port In: Boundary input for custom components ──────────────────────────────
class PortIn : public Component
{
public:
    explicit PortIn(int width = 1)
        : Component("PORT_IN", width, width, 1), label("in")
    {
        setBusWidth(width);
    }
    std::string label;
    uint64_t testValue = 0;
    
    void update() override {
        if (!getSimulator()) return;
        int w = getBusWidth();
        for (int i = 0; i < w; ++i) {
            if (receivers[i]->isConnected()) {
                drivers[i]->setState(receivers[i]->getState());
            } else {
                bool bitOn = (testValue >> i) & 1;
                drivers[i]->setState(bitOn ? driveTrue(*this) : driveFalse(*this));
            }
        }
    }
};

// ─── Port Out: Boundary output for custom components ──────────────────────────
class PortOut : public Component
{
public:
    explicit PortOut(int width = 1)
        : Component("PORT_OUT", width, width, 1), label("out")
    {
        setBusWidth(width);
    }
    std::string label;
    void update() override {
        if (!getSimulator()) return;
        int w = getBusWidth();
        for (int i = 0; i < w; ++i) {
            drivers[i]->setState(receivers[i]->getState());
        }
    }
};
