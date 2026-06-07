#pragma once
#include "Components.hpp"
#include "PowerRails.hpp"

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
        if (!getSimulator()) {
            drivers[0]->setState(on ? State::HIGH : State::LOW);
            return;
        }
        drivers[0]->setState(on ? driveTrue(*getSimulator())
                                : driveFalse(*getSimulator()));
    }

    bool  isOn() const { return outputOn; }
    State getOutputState() const { return drivers[0]->getState(); }

    void onRegistered() override { setOutput(outputOn); }

    void update() override {}

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
        if (!pressed) {
            pressed = true;
            if (!getSimulator()) drivers[0]->setState(State::HIGH);
            else drivers[0]->setState(driveTrue(*getSimulator()));
        }
    }
    void release()
    {
        pressed = false;
        if (!getSimulator()) {
            drivers[0]->setState(State::LOW);
            return;
        }
        drivers[0]->setState(driveFalse(*getSimulator()));
    }

    void onRegistered() override
    {
        if (pressed) press();
        else         release();
    }

    bool isPressed() const { return pressed; }
    void update() override {}

private:
    bool pressed = false;
};

// ─── Clock ────────────────────────────────────────────────────────────────────
class Clock : public Component
{
public:
    explicit Clock(int halfPeriodTicks = 10)
        : Component("CLK", 0, 1), halfPeriod(halfPeriodTicks)
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

    void update() override {}

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
        return readAsTrue(getLitState(), *getSimulator());
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
        auto& sim = *getSimulator();
        for (int i = 0; i < 4; ++i) {
            bool bit = (value >> (3 - i)) & 1;
            drivers[i]->setState(bit ? driveTrue(sim) : driveFalse(sim));
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
        auto& sim = *getSimulator();
        int v = 0;
        for (int i = 0; i < 4; ++i)
            if (readAsTrue(receivers[i]->getState(), sim))
                v |= (8 >> i);
        return v;
    }

    bool hasAmbiguity() const
    {
        if (!getSimulator()) return true;
        auto& sim = *getSimulator();
        for (int i = 0; i < 4; ++i) {
            State s = receivers[i]->getState();
            if (!readAsTrue(s, sim) && !readAsFalse(s, sim))
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
        auto& sim = *getSimulator();
        int w = getBusWidth();
        for (int i = 0; i < w; ++i) {
            State s = receivers[i]->getState();
            if (readAsTrue(s, sim))       drivers[i]->setState(driveTrue(sim));
            else if (readAsFalse(s, sim)) drivers[i]->setState(driveFalse(sim));
            else                          drivers[i]->setState(s);
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
        auto& sim = *getSimulator();
        int w = getBusWidth();
        for (int i = 0; i < w; ++i) {
            State s = receivers[i]->getState();
            if (readAsTrue(s, sim))       drivers[i]->setState(driveTrue(sim));
            else if (readAsFalse(s, sim)) drivers[i]->setState(driveFalse(sim));
            else                          drivers[i]->setState(s);
        }
    }
};
