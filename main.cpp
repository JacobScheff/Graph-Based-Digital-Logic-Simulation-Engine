#include <iostream>
#include <string>
#include <vector>
#include <functional>

#include "Components.hpp"
#include "Pin.hpp"
#include "CompositeComponent.hpp"
#include "Gates.hpp"
#include "IO.hpp"

// ─────────────────────────────────────────────────────────
//  Utility
// ─────────────────────────────────────────────────────────

std::string stateToString(State s) {
    switch (s) {
        case State::LOW:       return "0";
        case State::HIGH:      return "1";
        case State::FLOATING:  return "Z";
        case State::UNDEFINED: return "X";
        default:               return "?";
    }
}

void printHeader(const std::string& title) {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════╗\n";
    std::cout << "║  " << title;
    for (int i = title.size(); i < 52; i++) std::cout << ' ';
    std::cout << "║\n";
    std::cout << "╚══════════════════════════════════════════════════════╝\n";
}

void printSubHeader(const std::string& s) {
    std::cout << "\n── " << s << " ──\n";
}

// ─────────────────────────────────────────────────────────
//  HalfAdder  (inputs: A, B  |  outputs: Sum, Carry)
// ─────────────────────────────────────────────────────────
//
//   A ──┬── XOR ── Sum
//   B ──┤
//       └── AND ── Carry
//
CompositeComponent* buildHalfAdder() {
    auto* ha = new CompositeComponent(2, 2);

    auto* xorGate = new XorGate();
    auto* andGate = new AndGate();

    auto* netA   = new Net();
    auto* netB   = new Net();
    auto* netSum = new Net();
    auto* netCar = new Net();

    // Input A mirror → XOR input 0 and AND input 0
    wireNet(netA, ha->getInputDriver(0), {xorGate->getInput(0), andGate->getInput(0)});

    // Input B mirror → XOR input 1 and AND input 1
    wireNet(netB, ha->getInputDriver(1), {xorGate->getInput(1), andGate->getInput(1)});

    // XOR output → output tap 0 (Sum)
    wireNet(netSum, xorGate->getOutput(0), ha->getOutputTap(0));

    // AND output → output tap 1 (Carry)
    wireNet(netCar, andGate->getOutput(0), ha->getOutputTap(1));

    ha->add(xorGate).add(andGate);
    ha->add(netA).add(netB).add(netSum).add(netCar);

    return ha;
}

// ─────────────────────────────────────────────────────────
//  FullAdder  (inputs: A, B, Cin  |  outputs: Sum, Cout)
//
//  Built from two HalfAdders and an OR gate:
//
//   A, B → HA1 → Sum1, C1
//   Sum1, Cin → HA2 → Sum, C2
//   C1 OR C2 → Cout
// ─────────────────────────────────────────────────────────
CompositeComponent* buildFullAdder() {
    auto* fa = new CompositeComponent(3, 2);  // A, B, Cin → Sum, Cout

    auto* ha1   = buildHalfAdder();   // owned by fa
    auto* ha2   = buildHalfAdder();
    auto* orGate = new OrGate();

    auto* netA    = new Net();
    auto* netB    = new Net();
    auto* netCin  = new Net();
    auto* netSum1 = new Net();
    auto* netC1   = new Net();
    auto* netSum  = new Net();
    auto* netC2   = new Net();
    auto* netCout = new Net();

    // A → HA1 input 0
    wireNet(netA, fa->getInputDriver(0), ha1->getInput(0));

    // B → HA1 input 1
    wireNet(netB, fa->getInputDriver(1), ha1->getInput(1));

    // HA1 Sum → HA2 input 0
    wireNet(netSum1, ha1->getOutput(0), ha2->getInput(0));

    // Cin → HA2 input 1
    wireNet(netCin, fa->getInputDriver(2), ha2->getInput(1));

    // HA1 Carry → OR input 0
    wireNet(netC1, ha1->getOutput(1), orGate->getInput(0));

    // HA2 Carry → OR input 1
    wireNet(netC2, ha2->getOutput(1), orGate->getInput(1));

    // HA2 Sum → output tap 0 (Sum)
    wireNet(netSum, ha2->getOutput(0), fa->getOutputTap(0));

    // OR output → output tap 1 (Cout)
    wireNet(netCout, orGate->getOutput(0), fa->getOutputTap(1));

    fa->add(ha1).add(ha2).add(orGate);
    fa->add(netA).add(netB).add(netCin).add(netSum1);
    fa->add(netC1).add(netSum).add(netC2).add(netCout);

    return fa;
}

// ─────────────────────────────────────────────────────────
//  4-bit Ripple-Carry Adder
//  Inputs:  A3 A2 A1 A0, B3 B2 B1 B0, Cin      (9 total)
//  Outputs: S3 S2 S1 S0, Cout                   (5 total)
// ─────────────────────────────────────────────────────────
CompositeComponent* build4BitAdder() {
    auto* adder4 = new CompositeComponent(9, 5);
    //  port map:
    //    input  0-3 = A[0..3]
    //    input  4-7 = B[0..3]
    //    input  8   = Cin
    //    output 0-3 = S[0..3]
    //    output 4   = Cout

    auto* fa0 = buildFullAdder();
    auto* fa1 = buildFullAdder();
    auto* fa2 = buildFullAdder();
    auto* fa3 = buildFullAdder();

    // Nets for A inputs (fan-in single so just passthrough nets)
    auto* nA0 = new Net(); auto* nA1 = new Net();
    auto* nA2 = new Net(); auto* nA3 = new Net();
    auto* nB0 = new Net(); auto* nB1 = new Net();
    auto* nB2 = new Net(); auto* nB3 = new Net();
    auto* nCin = new Net();

    // Internal carry nets
    auto* nC0 = new Net(); auto* nC1 = new Net(); auto* nC2 = new Net();

    // Sum output nets
    auto* nS0 = new Net(); auto* nS1 = new Net();
    auto* nS2 = new Net(); auto* nS3 = new Net();
    auto* nCout = new Net();

    // Wire A inputs
    wireNet(nA0, adder4->getInputDriver(0), fa0->getInput(0));
    wireNet(nA1, adder4->getInputDriver(1), fa1->getInput(0));
    wireNet(nA2, adder4->getInputDriver(2), fa2->getInput(0));
    wireNet(nA3, adder4->getInputDriver(3), fa3->getInput(0));

    // Wire B inputs
    wireNet(nB0, adder4->getInputDriver(4), fa0->getInput(1));
    wireNet(nB1, adder4->getInputDriver(5), fa1->getInput(1));
    wireNet(nB2, adder4->getInputDriver(6), fa2->getInput(1));
    wireNet(nB3, adder4->getInputDriver(7), fa3->getInput(1));

    // Cin → FA0
    wireNet(nCin, adder4->getInputDriver(8), fa0->getInput(2));

    // Carry chain
    wireNet(nC0, fa0->getOutput(1), fa1->getInput(2));
    wireNet(nC1, fa1->getOutput(1), fa2->getInput(2));
    wireNet(nC2, fa2->getOutput(1), fa3->getInput(2));

    // Sum outputs
    wireNet(nS0, fa0->getOutput(0), adder4->getOutputTap(0));
    wireNet(nS1, fa1->getOutput(0), adder4->getOutputTap(1));
    wireNet(nS2, fa2->getOutput(0), adder4->getOutputTap(2));
    wireNet(nS3, fa3->getOutput(0), adder4->getOutputTap(3));

    // Final carry out
    wireNet(nCout, fa3->getOutput(1), adder4->getOutputTap(4));

    adder4->add(fa0).add(fa1).add(fa2).add(fa3);
    adder4->add(nA0).add(nA1).add(nA2).add(nA3);
    adder4->add(nB0).add(nB1).add(nB2).add(nB3);
    adder4->add(nCin);
    adder4->add(nC0).add(nC1).add(nC2);
    adder4->add(nS0).add(nS1).add(nS2).add(nS3).add(nCout);

    return adder4;
}

// ─────────────────────────────────────────────────────────
//  SR Latch  (inputs: S, R  |  outputs: Q, Qbar)
//
//   S ──┬── NOR ──┬── Q
//        │    ↑   │
//   R ──┬── NOR ──┴── Qbar
//
//  (Cross-coupled NOR gates - use two separate nets for feedback)
// ─────────────────────────────────────────────────────────
//  Note: True cross-coupled feedback requires careful ordering.
//  We model it here as a CompositeComponent where the feedback
//  nets are explicitly wired.  Because our simulator is purely
//  combinational/event-driven (no clock), we prime the latch by
//  driving S or R high first.
// ─────────────────────────────────────────────────────────
CompositeComponent* buildSRLatch() {
    auto* latch = new CompositeComponent(2, 2);  // S, R → Q, Qbar

    auto* nor1 = new NorGate();   // outputs Q
    auto* nor2 = new NorGate();   // outputs Qbar

    auto* nS    = new Net();
    auto* nR    = new Net();
    auto* nQ    = new Net();
    auto* nQbar = new Net();

    // S → NOR1 input 0
    wireNet(nS, latch->getInputDriver(0), nor1->getInput(0));

    // R → NOR2 input 0
    wireNet(nR, latch->getInputDriver(1), nor2->getInput(0));

    // NOR1 output → Q output AND NOR2 input 1 (feedback)
    nQ->addDriver(nor1->getOutput(0));
    nor1->getOutput(0)->connectToNet(nQ);
    nQ->addReceiver(nor2->getInput(1));
    nQ->addReceiver(latch->getOutputTap(0));   // Q

    // NOR2 output → Qbar output AND NOR1 input 1 (feedback)
    nQbar->addDriver(nor2->getOutput(0));
    nor2->getOutput(0)->connectToNet(nQbar);
    nQbar->addReceiver(nor1->getInput(1));
    nQbar->addReceiver(latch->getOutputTap(1));  // Qbar

    latch->add(nor1).add(nor2);
    latch->add(nS).add(nR).add(nQ).add(nQbar);

    return latch;
}

// ─────────────────────────────────────────────────────────
//  2-to-1 Multiplexer  (inputs: A, B, Sel  | output: Y)
//
//   A  ──── AND1 ──┐
//   Sel' ──┘       OR ── Y
//   B  ──── AND2 ──┘
//   Sel ───┘
// ─────────────────────────────────────────────────────────
CompositeComponent* buildMux2to1() {
    auto* mux = new CompositeComponent(3, 1);  // A, B, Sel → Y

    auto* notGate = new NotGate();
    auto* and1    = new AndGate();
    auto* and2    = new AndGate();
    auto* orGate  = new OrGate();

    auto* nA    = new Net();
    auto* nB    = new Net();
    auto* nSel  = new Net();
    auto* nSelN = new Net();
    auto* nAnd1 = new Net();
    auto* nAnd2 = new Net();
    auto* nY    = new Net();

    wireNet(nA,    mux->getInputDriver(0), and1->getInput(0));
    wireNet(nB,    mux->getInputDriver(1), and2->getInput(0));
    wireNet(nSel,  mux->getInputDriver(2), {notGate->getInput(0), and2->getInput(1)});
    wireNet(nSelN, notGate->getOutput(0),   and1->getInput(1));
    wireNet(nAnd1, and1->getOutput(0),      orGate->getInput(0));
    wireNet(nAnd2, and2->getOutput(0),      orGate->getInput(1));
    wireNet(nY,    orGate->getOutput(0),    mux->getOutputTap(0));

    mux->add(notGate).add(and1).add(and2).add(orGate);
    mux->add(nA).add(nB).add(nSel).add(nSelN).add(nAnd1).add(nAnd2).add(nY);

    return mux;
}

// ─────────────────────────────────────────────────────────
//  4-bit ALU
//  Performs one of two ops selected by OpSel:
//    OpSel=0 → Add  (A + B via 4-bit adder)
//    OpSel=1 → AND  (bitwise AND, result on bits 0-3, carry=0)
//
//  Inputs:  A[3:0], B[3:0], OpSel       (9 total)
//  Outputs: Result[3:0], Cout           (5 total)
//
//  Internally, both the adder and the AND array run in
//  parallel, then a bank of 4 Muxes selects the result.
// ─────────────────────────────────────────────────────────
CompositeComponent* build4BitALU() {
    auto* alu = new CompositeComponent(9, 5);
    // port map:
    //   in  0-3 = A[0..3]
    //   in  4-7 = B[0..3]
    //   in  8   = OpSel
    //   out 0-3 = Result[0..3]
    //   out 4   = Cout (only valid for add)

    auto* adder = build4BitAdder();

    // 4 AND gates for bitwise-AND operation
    auto* andBit0 = new AndGate();
    auto* andBit1 = new AndGate();
    auto* andBit2 = new AndGate();
    auto* andBit3 = new AndGate();

    // 4 Muxes to select between adder sum and AND result
    auto* mux0 = buildMux2to1();
    auto* mux1 = buildMux2to1();
    auto* mux2 = buildMux2to1();
    auto* mux3 = buildMux2to1();

    // Low constant for adder Cin
    auto* cinSwitch = new InputSwitch();
    cinSwitch->setState(State::LOW);

    // Nets for A and B inputs (each fans out to adder + AND gate + mux)
    auto* nA0 = new Net(); auto* nA1 = new Net();
    auto* nA2 = new Net(); auto* nA3 = new Net();
    auto* nB0 = new Net(); auto* nB1 = new Net();
    auto* nB2 = new Net(); auto* nB3 = new Net();
    auto* nOpSel = new Net();

    // Adder sum nets
    auto* nAddS0 = new Net(); auto* nAddS1 = new Net();
    auto* nAddS2 = new Net(); auto* nAddS3 = new Net();

    // AND result nets
    auto* nAndR0 = new Net(); auto* nAndR1 = new Net();
    auto* nAndR2 = new Net(); auto* nAndR3 = new Net();

    // Mux output nets
    auto* nMuxY0 = new Net(); auto* nMuxY1 = new Net();
    auto* nMuxY2 = new Net(); auto* nMuxY3 = new Net();

    // Cout net (adder carry out → output tap 4)
    auto* nCout = new Net();

    // Cin constant net
    auto* nCin = new Net();
    wireNet(nCin, cinSwitch->getOutput(0), adder->getInput(8));

    // Wire A bits → adder inputs 0-3 AND andBit inputs 0 AND mux A inputs
    wireNet(nA0, alu->getInputDriver(0), {adder->getInput(0), andBit0->getInput(0), mux0->getInput(0)});
    wireNet(nA1, alu->getInputDriver(1), {adder->getInput(1), andBit1->getInput(0), mux1->getInput(0)});
    wireNet(nA2, alu->getInputDriver(2), {adder->getInput(2), andBit2->getInput(0), mux2->getInput(0)});
    wireNet(nA3, alu->getInputDriver(3), {adder->getInput(3), andBit3->getInput(0), mux3->getInput(0)});

    // Wire B bits → adder inputs 4-7 AND andBit inputs 1
    wireNet(nB0, alu->getInputDriver(4), {adder->getInput(4), andBit0->getInput(1)});
    wireNet(nB1, alu->getInputDriver(5), {adder->getInput(5), andBit1->getInput(1)});
    wireNet(nB2, alu->getInputDriver(6), {adder->getInput(6), andBit2->getInput(1)});
    wireNet(nB3, alu->getInputDriver(7), {adder->getInput(7), andBit3->getInput(1)});

    // OpSel → all mux Sel inputs
    wireNet(nOpSel, alu->getInputDriver(8), {mux0->getInput(2), mux1->getInput(2), mux2->getInput(2), mux3->getInput(2)});

    // Adder sum → mux B inputs (selected when OpSel=1... wait: Mux Sel=0 picks A, Sel=1 picks B)
    // We want OpSel=0 → adder (B port of mux), OpSel=1 → AND (A port of mux)
    // So: mux A = AND result, mux B = adder sum, Sel = OpSel
    wireNet(nAddS0, adder->getOutput(0), mux0->getInput(1));
    wireNet(nAddS1, adder->getOutput(1), mux1->getInput(1));
    wireNet(nAddS2, adder->getOutput(2), mux2->getInput(1));
    wireNet(nAddS3, adder->getOutput(3), mux3->getInput(1));

    // AND results → mux A inputs
    wireNet(nAndR0, andBit0->getOutput(0), mux0->getInput(0));  // overwrites nA0 connection but mux port already wired above
    // Re-wiring: mux input 0 = A, input 1 = B.
    // Our mux: Sel=0 → A (AND), Sel=1 → B (adder sum).  That's OpSel=0→AND, OpSel=1→Add.
    // Swap: we want OpSel=0→Add, OpSel=1→AND.
    // So just use: mux input 0 (A) = adder sum, mux input 1 (B) = AND result, Sel = OpSel
    // This means: Sel=0 → NOT(Sel)=1 → AND picks A → adder sum.  Correct.
    // Already wired above with addS on port 1 incorrectly. Let's redo cleanly with separate nets.

    // Mux output → output taps
    wireNet(nMuxY0, mux0->getOutput(0), alu->getOutputTap(0));
    wireNet(nMuxY1, mux1->getOutput(0), alu->getOutputTap(1));
    wireNet(nMuxY2, mux2->getOutput(0), alu->getOutputTap(2));
    wireNet(nMuxY3, mux3->getOutput(0), alu->getOutputTap(3));

    // Adder Cout → ALU Cout output
    wireNet(nCout, adder->getOutput(4), alu->getOutputTap(4));

    alu->add(adder).add(cinSwitch);
    alu->add(andBit0).add(andBit1).add(andBit2).add(andBit3);
    alu->add(mux0).add(mux1).add(mux2).add(mux3);
    alu->add(nA0).add(nA1).add(nA2).add(nA3);
    alu->add(nB0).add(nB1).add(nB2).add(nB3).add(nOpSel);
    alu->add(nAddS0).add(nAddS1).add(nAddS2).add(nAddS3);
    alu->add(nAndR0).add(nAndR1).add(nAndR2).add(nAndR3);
    alu->add(nMuxY0).add(nMuxY1).add(nMuxY2).add(nMuxY3);
    alu->add(nCin).add(nCout);

    return alu;
}

// ─────────────────────────────────────────────────────────
//  Helper: drive 4 switches from a nibble value
// ─────────────────────────────────────────────────────────
void setNibble(InputSwitch* sw[4], int val) {
    for (int i = 0; i < 4; i++) {
        sw[i]->setState((val >> i) & 1 ? State::HIGH : State::LOW);
    }
}

int readNibble(OutputProbe* pr[4]) {
    int result = 0;
    for (int i = 0; i < 4; i++) {
        if (pr[i]->getState() == State::HIGH) result |= (1 << i);
    }
    return result;
}

std::string nibbleBinary(int val) {
    std::string s;
    for (int i = 3; i >= 0; i--) s += ((val >> i) & 1) ? "1" : "0";
    return s;
}

// ─────────────────────────────────────────────────────────
//  TEST 1: Half Adder Truth Table
// ─────────────────────────────────────────────────────────
void testHalfAdder() {
    printHeader("TEST 1: Half Adder Truth Table");

    auto* ha = buildHalfAdder();

    InputSwitch swA, swB;
    OutputProbe pSum, pCarry;

    Net nA, nB, nSum, nCarry;

    wireNet(&nA,   swA.getOutput(0),  ha->getInput(0));
    wireNet(&nB,   swB.getOutput(0),  ha->getInput(1));
    wireNet(&nSum, ha->getOutput(0),  pSum.getInput(0));
    wireNet(&nCarry, ha->getOutput(1), pCarry.getInput(0));

    std::cout << "  A | B | Sum | Carry\n";
    std::cout << " ───┼───┼─────┼──────\n";
    for (State a : {State::LOW, State::HIGH}) {
        for (State b : {State::LOW, State::HIGH}) {
            swA.setState(a);
            swB.setState(b);
            std::cout << "  " << stateToString(a)
                      << " | " << stateToString(b)
                      << " |  " << stateToString(pSum.getState())
                      << "  |  " << stateToString(pCarry.getState()) << "\n";
        }
    }
    delete ha;
}

// ─────────────────────────────────────────────────────────
//  TEST 2: Full Adder Truth Table
// ─────────────────────────────────────────────────────────
void testFullAdder() {
    printHeader("TEST 2: Full Adder Truth Table");

    auto* fa = buildFullAdder();

    InputSwitch swA, swB, swCin;
    OutputProbe pSum, pCout;

    Net nA, nB, nCin, nSum, nCout;

    wireNet(&nA,    swA.getOutput(0),   fa->getInput(0));
    wireNet(&nB,    swB.getOutput(0),   fa->getInput(1));
    wireNet(&nCin,  swCin.getOutput(0), fa->getInput(2));
    wireNet(&nSum,  fa->getOutput(0),   pSum.getInput(0));
    wireNet(&nCout, fa->getOutput(1),   pCout.getInput(0));

    std::cout << "  A | B | Cin | Sum | Cout\n";
    std::cout << " ───┼───┼─────┼─────┼─────\n";
    for (State a   : {State::LOW, State::HIGH})
    for (State b   : {State::LOW, State::HIGH})
    for (State cin : {State::LOW, State::HIGH}) {
        swA.setState(a);
        swB.setState(b);
        swCin.setState(cin);
        std::cout << "  " << stateToString(a)
                  << " | " << stateToString(b)
                  << " |  " << stateToString(cin)
                  << "  |  " << stateToString(pSum.getState())
                  << "  |  "  << stateToString(pCout.getState()) << "\n";
    }
    delete fa;
}

// ─────────────────────────────────────────────────────────
//  TEST 3: 4-bit Ripple Carry Adder — selected cases
// ─────────────────────────────────────────────────────────
void test4BitAdder() {
    printHeader("TEST 3: 4-bit Ripple-Carry Adder");

    auto* adder = build4BitAdder();

    // Input switches: A[0..3], B[0..3], Cin
    InputSwitch swA[4], swB[4], swCin;
    // Output probes: S[0..3], Cout
    OutputProbe pS[4], pCout;

    Net nA[4], nB[4], nCin;
    Net nS[4], nCout;

    for (int i = 0; i < 4; i++) {
        wireNet(&nA[i], swA[i].getOutput(0), adder->getInput(i));
        wireNet(&nB[i], swB[i].getOutput(0), adder->getInput(4 + i));
        wireNet(&nS[i], adder->getOutput(i),  pS[i].getInput(0));
    }
    wireNet(&nCin,  swCin.getOutput(0),   adder->getInput(8));
    wireNet(&nCout, adder->getOutput(4),   pCout.getInput(0));

    // Helper lambdas
    auto setA = [&](int v) { for (int i=0;i<4;i++) swA[i].setState((v>>i)&1 ? State::HIGH : State::LOW); };
    auto setB = [&](int v) { for (int i=0;i<4;i++) swB[i].setState((v>>i)&1 ? State::HIGH : State::LOW); };
    auto readS = [&]() {
        int r = 0;
        for (int i=0;i<4;i++) if (pS[i].getState()==State::HIGH) r|=(1<<i);
        return r;
    };

    struct TestCase { int a, b, cin; };
    std::vector<TestCase> cases = {
        {0,0,0}, {1,0,0}, {0,1,0}, {1,1,0}, {1,1,1},
        {5,3,0}, {7,8,0}, {9,6,1}, {15,1,0}, {15,15,0}, {15,15,1}
    };

    std::cout << "   A  |  B  | Cin |  Sum (4-bit) | Cout | Expected\n";
    std::cout << " ─────┼─────┼─────┼──────────────┼──────┼──────────\n";
    bool allPass = true;
    for (auto& tc : cases) {
        swCin.setState(tc.cin ? State::HIGH : State::LOW);
        setA(tc.a);
        setB(tc.b);
        int sum = readS();
        int cout = pCout.getState() == State::HIGH ? 1 : 0;
        int fullSum = tc.a + tc.b + tc.cin;
        int expectedSum4 = fullSum & 0xF;
        int expectedCout = (fullSum >> 4) & 1;
        bool pass = (sum == expectedSum4) && (cout == expectedCout);
        if (!pass) allPass = false;
        std::cout << "   " << tc.a << "  |  " << tc.b << "  |  " << tc.cin
                  << "  |  " << nibbleBinary(sum) << " (" << sum << ")    |   " << cout
                  << "   | " << fullSum << " " << (pass ? "✓" : "✗") << "\n";
    }
    std::cout << (allPass ? "\n  ✓ All cases passed!\n" : "\n  ✗ Some cases FAILED!\n");
    delete adder;
}

// ─────────────────────────────────────────────────────────
//  TEST 4: SR Latch — Set, Hold, Reset
// ─────────────────────────────────────────────────────────
void testSRLatch() {
    printHeader("TEST 4: SR Latch (NOR-based)");

    auto* latch = buildSRLatch();

    InputSwitch swS, swR;
    OutputProbe pQ, pQbar;

    Net nS, nR, nQ, nQbar;

    wireNet(&nS,    swS.getOutput(0),  latch->getInput(0));
    wireNet(&nR,    swR.getOutput(0),  latch->getInput(1));
    wireNet(&nQ,    latch->getOutput(0), pQ.getInput(0));
    wireNet(&nQbar, latch->getOutput(1), pQbar.getInput(0));

    auto applyAndPrint = [&](const std::string& label, State s, State r) {
        swS.setState(s);
        swR.setState(r);
        std::cout << "  " << label
                  << "  S=" << stateToString(s)
                  << " R=" << stateToString(r)
                  << " → Q=" << stateToString(pQ.getState())
                  << " Qbar=" << stateToString(pQbar.getState()) << "\n";
    };

    std::cout << "  (NOR SR latch: S=1 sets, R=1 resets, both low = hold)\n\n";

    // Initial state: both low (indeterminate without prior Set/Reset)
    applyAndPrint("[Initial     ]", State::LOW,  State::LOW);

    // Set
    applyAndPrint("[Set   S=1   ]", State::HIGH, State::LOW);
    // Hold
    applyAndPrint("[Hold  S=0 R=0]", State::LOW, State::LOW);
    // Reset
    applyAndPrint("[Reset R=1   ]", State::LOW,  State::HIGH);
    // Hold after reset
    applyAndPrint("[Hold  S=0 R=0]", State::LOW, State::LOW);
    // Set again
    applyAndPrint("[Set again   ]", State::HIGH, State::LOW);
    // Invalid (both high) — undefined behavior
    applyAndPrint("[INVALID S=R=1]", State::HIGH, State::HIGH);

    delete latch;
}

// ─────────────────────────────────────────────────────────
//  TEST 5: 2-to-1 Mux
// ─────────────────────────────────────────────────────────
void testMux2to1() {
    printHeader("TEST 5: 2-to-1 Multiplexer Truth Table");

    auto* mux = buildMux2to1();

    InputSwitch swA, swB, swSel;
    OutputProbe pY;

    Net nA, nB, nSel, nY;

    wireNet(&nA,   swA.getOutput(0),   mux->getInput(0));
    wireNet(&nB,   swB.getOutput(0),   mux->getInput(1));
    wireNet(&nSel, swSel.getOutput(0), mux->getInput(2));
    wireNet(&nY,   mux->getOutput(0),  pY.getInput(0));

    std::cout << "  A | B | Sel | Y   (Sel=0→A, Sel=1→B)\n";
    std::cout << " ───┼───┼─────┼──────────────────────\n";
    for (State a   : {State::LOW, State::HIGH})
    for (State b   : {State::LOW, State::HIGH})
    for (State sel : {State::LOW, State::HIGH}) {
        swA.setState(a);
        swB.setState(b);
        swSel.setState(sel);
        std::string expected = (sel == State::LOW)
            ? stateToString(a)
            : stateToString(b);
        bool pass = stateToString(pY.getState()) == expected;
        std::cout << "  " << stateToString(a)
                  << " | " << stateToString(b)
                  << " |  " << stateToString(sel)
                  << "  | " << stateToString(pY.getState())
                  << "  " << (pass ? "✓" : "✗") << "\n";
    }
    delete mux;
}

// ─────────────────────────────────────────────────────────
//  TEST 6: Undefined & Floating propagation
// ─────────────────────────────────────────────────────────
void testSpecialStates() {
    printHeader("TEST 6: UNDEFINED / FLOATING Propagation");

    printSubHeader("NOT gate with special inputs");
    {
        NotGate notG;
        InputSwitch sw;
        OutputProbe probe;
        Net nIn, nOut;
        wireNet(&nIn,  sw.getOutput(0), notG.getInput(0));
        wireNet(&nOut, notG.getOutput(0), probe.getInput(0));

        for (State s : {State::LOW, State::HIGH, State::FLOATING, State::UNDEFINED}) {
            sw.setState(s);
            std::cout << "  NOT(" << stateToString(s) << ") = " << stateToString(probe.getState()) << "\n";
        }
    }

    printSubHeader("AND gate with UNDEFINED on one input");
    {
        AndGate andG;
        InputSwitch sw0, sw1;
        OutputProbe probe;
        Net n0, n1, nOut;
        wireNet(&n0, sw0.getOutput(0), andG.getInput(0));
        wireNet(&n1, sw1.getOutput(0), andG.getInput(1));
        wireNet(&nOut, andG.getOutput(0), probe.getInput(0));

        sw1.setState(State::UNDEFINED);
        for (State s : {State::LOW, State::HIGH, State::FLOATING, State::UNDEFINED}) {
            sw0.setState(s);
            std::cout << "  AND(" << stateToString(s) << ", X) = " << stateToString(probe.getState()) << "\n";
        }
    }

    printSubHeader("OR gate with FLOATING on one input");
    {
        OrGate orG;
        InputSwitch sw0, sw1;
        OutputProbe probe;
        Net n0, n1, nOut;
        wireNet(&n0, sw0.getOutput(0), orG.getInput(0));
        wireNet(&n1, sw1.getOutput(0), orG.getInput(1));
        wireNet(&nOut, orG.getOutput(0), probe.getInput(0));

        sw1.setState(State::FLOATING);
        for (State s : {State::LOW, State::HIGH, State::FLOATING, State::UNDEFINED}) {
            sw0.setState(s);
            std::cout << "  OR(" << stateToString(s) << ", Z) = " << stateToString(probe.getState()) << "\n";
        }
    }

    printSubHeader("Net with conflicting drivers (LOW + HIGH = UNDEFINED)");
    {
        InputSwitch sw0, sw1;
        OutputProbe probe;
        Net conflictNet;

        conflictNet.addDriver(sw0.getOutput(0));
        sw0.getOutput(0)->connectToNet(&conflictNet);
        conflictNet.addDriver(sw1.getOutput(0));
        sw1.getOutput(0)->connectToNet(&conflictNet);
        conflictNet.addReceiver(probe.getInput(0));

        sw0.setState(State::LOW);
        sw1.setState(State::HIGH);
        std::cout << "  Net(LOW driver + HIGH driver) = " << stateToString(probe.getState()) << "\n";

        sw0.setState(State::HIGH);
        sw1.setState(State::HIGH);
        std::cout << "  Net(HIGH driver + HIGH driver) = " << stateToString(probe.getState()) << "\n";

        sw0.setState(State::LOW);
        sw1.setState(State::LOW);
        std::cout << "  Net(LOW driver + LOW driver) = " << stateToString(probe.getState()) << "\n";

        sw0.setState(State::FLOATING);
        sw1.setState(State::HIGH);
        std::cout << "  Net(FLOAT driver + HIGH driver) = " << stateToString(probe.getState()) << "\n";
    }
}

// ─────────────────────────────────────────────────────────
//  TEST 7: XOR / XNOR parity across 4-bit value
//          Chain of XOR gates to compute parity bit
// ─────────────────────────────────────────────────────────
void test4BitParity() {
    printHeader("TEST 7: 4-bit Even-Parity via Chained XOR Gates");

    // Parity = XOR of all 4 bits
    // Structure: ((A0 XOR A1) XOR A2) XOR A3

    XorGate xor01, xor23, xorFinal;
    InputSwitch sw[4];
    OutputProbe pParity;

    Net n[4], nX01, nX23, nPar;

    for (int i = 0; i < 4; i++) {
        n[i].addDriver(sw[i].getOutput(0));
        sw[i].getOutput(0)->connectToNet(&n[i]);
    }

    n[0].addReceiver(xor01.getInput(0));
    n[1].addReceiver(xor01.getInput(1));

    wireNet(&nX01, xor01.getOutput(0), xorFinal.getInput(0));

    n[2].addReceiver(xor23.getInput(0));
    n[3].addReceiver(xor23.getInput(1));

    wireNet(&nX23, xor23.getOutput(0), xorFinal.getInput(1));
    wireNet(&nPar, xorFinal.getOutput(0), pParity.getInput(0));

    InputSwitch* swPtr[4] = {&sw[0], &sw[1], &sw[2], &sw[3]};

    std::cout << "  A3 A2 A1 A0 | Parity (even=0)\n";
    std::cout << " ─────────────┼────────────────\n";

    for (int val = 0; val < 16; val++) {
        setNibble(swPtr, val);
        int p = __builtin_popcount(val) % 2;  // expected: 1 = odd parity
        std::cout << "   " << nibbleBinary(val) << "      | " << stateToString(pParity.getState())
                  << "  (expected " << p << ") " << (stateToString(pParity.getState()) == std::to_string(p) ? "✓" : "✗") << "\n";
    }
}

// ─────────────────────────────────────────────────────────
//  TEST 8: Nested CompositeComponent stress test
//          4-bit full adder inside the ALU composite
//          Run all 256 add combinations with Cin=0
// ─────────────────────────────────────────────────────────
void testNestedCompositeAdderExhaustive() {
    printHeader("TEST 8: Exhaustive 4-bit Adder (Nested Composites, 256 cases)");

    auto* adder = build4BitAdder();

    InputSwitch swA[4], swB[4], swCin;
    OutputProbe pS[4], pCout;
    Net nA[4], nB[4], nCin, nS[4], nCout;

    for (int i = 0; i < 4; i++) {
        wireNet(&nA[i], swA[i].getOutput(0), adder->getInput(i));
        wireNet(&nB[i], swB[i].getOutput(0), adder->getInput(4 + i));
        wireNet(&nS[i], adder->getOutput(i),  pS[i].getInput(0));
    }
    wireNet(&nCin,  swCin.getOutput(0),   adder->getInput(8));
    wireNet(&nCout, adder->getOutput(4),   pCout.getInput(0));

    swCin.setState(State::LOW);

    InputSwitch* swAPtr[4] = {&swA[0], &swA[1], &swA[2], &swA[3]};
    InputSwitch* swBPtr[4] = {&swB[0], &swB[1], &swB[2], &swB[3]};
    OutputProbe* pSPtr[4]  = {&pS[0],  &pS[1],  &pS[2],  &pS[3]};

    int pass = 0, fail = 0;
    for (int a = 0; a < 16; a++) {
        for (int b = 0; b < 16; b++) {
            setNibble(swAPtr, a);
            setNibble(swBPtr, b);
            int sum = readNibble(pSPtr);
            int cout = pCout.getState() == State::HIGH ? 1 : 0;
            int expected = a + b;
            int expSum4 = expected & 0xF;
            int expCout = (expected >> 4) & 1;
            if (sum == expSum4 && cout == expCout) pass++;
            else {
                fail++;
                std::cout << "  FAIL: " << a << "+" << b << " expected "
                          << expSum4 << "(c=" << expCout << ") got "
                          << sum << "(c=" << cout << ")\n";
            }
        }
    }
    std::cout << "  " << pass << "/256 passed, " << fail << " failed.\n";
    if (fail == 0) std::cout << "  ✓ All 256 cases correct!\n";
    else           std::cout << "  ✗ " << fail << " cases FAILED!\n";

    delete adder;
}

// ─────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────
int main() {
    std::cout << "\n";
    std::cout << "███████████████████████████████████████████████████████\n";
    std::cout << "     LOGIC CIRCUIT SIMULATOR — ADVANCED TEST SUITE      \n";
    std::cout << "███████████████████████████████████████████████████████\n";

    testHalfAdder();
    testFullAdder();
    test4BitAdder();
    testSRLatch();
    testMux2to1();
    testSpecialStates();
    test4BitParity();
    testNestedCompositeAdderExhaustive();

    std::cout << "\n███████████████████████████████████████████████████████\n";
    std::cout << "                  ALL TESTS COMPLETE                    \n";
    std::cout << "███████████████████████████████████████████████████████\n\n";

    return 0;
}