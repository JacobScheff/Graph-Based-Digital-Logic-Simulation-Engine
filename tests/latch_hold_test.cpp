#include "Simulator.hpp"
#include "Gates.hpp"
#include "PowerRails.hpp"
#include <cstdio>
#include <memory>
#include <vector>

namespace {

struct XnorPairHarness
{
    std::unique_ptr<XnorGate> x0;
    std::unique_ptr<XnorGate> x1;

    XnorPairHarness()
        : x0(std::make_unique<XnorGate>())
        , x1(std::make_unique<XnorGate>())
    {}
};

bool isDefined(State s)
{
    return s != State::FLOATING && s != State::UNDEFINED;
}

void wirePair(Simulator& sim, XnorPairHarness& pair, Net* gndNet)
{
    sim.registerComponent(pair.x0.get());
    sim.registerComponent(pair.x1.get());

    Net* q0 = sim.createNet();
    Net* q1 = sim.createNet();

    sim.connectDriver(pair.x0->getDriver(0), q0);
    sim.connectDriver(pair.x1->getDriver(0), q1);

    sim.connectReceiver(pair.x0->getReceiver(0), q1);
    sim.connectReceiver(pair.x0->getReceiver(1), gndNet);
    sim.connectReceiver(pair.x1->getReceiver(0), q0);
    sim.connectReceiver(pair.x1->getReceiver(1), gndNet);
}

bool allPairsDefined(Simulator& sim, const std::vector<XnorPairHarness>& pairs)
{
    for (const auto& pair : pairs) {
        State s0 = pair.x0->getDriver(0)->getState();
        State s1 = pair.x1->getDriver(0)->getState();
        if (!isDefined(s0) || !isDefined(s1))
            return false;
    }
    return true;
}

} // namespace

int main()
{
    Simulator sim;
    Net* gndNet = sim.getGndNet();

    std::vector<XnorPairHarness> pairs(8);
    for (auto& pair : pairs)
        wirePair(sim, pair, gndNet);

    sim.settle();

    if (!allPairsDefined(sim, pairs)) {
        std::fprintf(stderr, "one or more XNOR pairs remained undefined after settle\n");
        return 1;
    }

    std::printf("latch_hold_test: ok (%zu pairs)\n", pairs.size());
    return 0;
}
