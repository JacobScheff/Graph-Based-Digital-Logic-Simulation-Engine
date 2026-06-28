#include "Simulator.hpp"
#include "Gates.hpp"
#include "IO.hpp"
#include "PowerRails.hpp"
#include <cstdio>
#include <memory>
#include <vector>

namespace {

bool isHigh(State s, const Component& c) { return readAsTrue(s, c); }

struct DLatch {
    Simulator sim;
    PortIn data{1};
    PortIn enable{1};
    PortOut out{1};
    NotGate notData;
    AndGate topAnd;
    AndGate bottomAnd;
    XnorGate topXnor;
    XnorGate bottomXnor;

    DLatch()
    {
        data.label = "Data";
        enable.label = "Enable";
        out.label = "out";

        sim.registerComponent(&data);
        sim.registerComponent(&enable);
        sim.registerComponent(&notData);
        sim.registerComponent(&topAnd);
        sim.registerComponent(&bottomAnd);
        sim.registerComponent(&topXnor);
        sim.registerComponent(&bottomXnor);
        sim.registerComponent(&out);

        Net* dataNet = sim.connectDriver(data.getDriver(0));
        Net* enableNet = sim.connectDriver(enable.getDriver(0));
        Net* notNet = sim.connectDriver(notData.getDriver(0));
        Net* topAndNet = sim.connectDriver(topAnd.getDriver(0));
        Net* bottomAndNet = sim.connectDriver(bottomAnd.getDriver(0));
        sim.connectDriver(topXnor.getDriver(0));
        sim.connectDriver(bottomXnor.getDriver(0));
        sim.connectDriver(out.getDriver(0));

        sim.connectReceiver(notData.getReceiver(0), dataNet);
        sim.connectReceiver(topAnd.getReceiver(0), enableNet);
        sim.connectReceiver(topAnd.getReceiver(1), notNet);
        sim.connectReceiver(bottomAnd.getReceiver(0), dataNet);
        sim.connectReceiver(bottomAnd.getReceiver(1), enableNet);
        sim.connectReceiver(topXnor.getReceiver(0), topAndNet);
        sim.connectReceiver(topXnor.getReceiver(1), bottomXnor.getDriver(0)->getNet());
        sim.connectReceiver(bottomXnor.getReceiver(0), bottomAndNet);
        sim.connectReceiver(bottomXnor.getReceiver(1), topXnor.getDriver(0)->getNet());
        sim.connectReceiver(out.getReceiver(0), topXnor.getDriver(0)->getNet());
    }

    void setInputs(bool dataOn, bool enableOn)
    {
        data.testValue = dataOn ? 1 : 0;
        enable.testValue = enableOn ? 1 : 0;
        data.update();
        enable.update();
    }

    bool outputHigh()
    {
        out.update();
        return isHigh(out.getDriver(0)->getState(), out);
    }

    void settle() { sim.settle(); }
};

bool expect(const char* label, bool actual, bool expected)
{
    if (actual == expected) return true;
    std::fprintf(stderr, "FAIL: %s expected %d got %d\n", label, expected, actual);
    return false;
}

} // namespace

int main()
{
    DLatch latch;
    bool ok = true;

    latch.setInputs(false, true);
    latch.settle();
    ok &= expect("transparent load 0", latch.outputHigh(), false);

    latch.setInputs(false, false);
    latch.settle();
    ok &= expect("hold 0 after enable drop", latch.outputHigh(), false);

    latch.setInputs(true, true);
    latch.settle();
    ok &= expect("transparent load 1", latch.outputHigh(), true);

    latch.setInputs(true, false);
    latch.settle();
    ok &= expect("hold 1 after enable drop", latch.outputHigh(), true);

    if (ok) {
        std::printf("All latch hold tests passed.\n");
        return 0;
    }
    return 1;
}
