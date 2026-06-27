// Regression test: nested custom components must resolve during registerInternals.
#include "Simulator.hpp"
#include "Canvas.hpp"
#include "IO.hpp"
#include "CustomComponent.hpp"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

using json = nlohmann::json;

static bool loadCustomDef(const std::string& path, CustomComponentDef& out)
{
    std::ifstream f(path);
    if (!f.is_open()) return false;
    json j;
    f >> j;
    out.typeName = j.value("typeName", "Custom");
    out.width = j.value("width", 100);
    out.height = j.value("height", 100);
    out.canvasJson = j.value("canvasJson", "");
    out.inPorts.clear();
    out.outPorts.clear();
    if (j.contains("inPorts")) {
        for (auto& jp : j["inPorts"]) {
            CustomPortDef p;
            p.internalCompId = jp.value("internalCompId", -1);
            p.label = jp.value("label", "in");
            p.busWidth = jp.value("busWidth", 1);
            p.side = jp.value("side", 0);
            p.order = jp.value("order", 0);
            out.inPorts.push_back(p);
        }
    }
    if (j.contains("outPorts")) {
        for (auto& jp : j["outPorts"]) {
            CustomPortDef p;
            p.internalCompId = jp.value("internalCompId", -1);
            p.label = jp.value("label", "out");
            p.busWidth = jp.value("busWidth", 1);
            p.side = jp.value("side", 0);
            p.order = jp.value("order", 0);
            out.outPorts.push_back(p);
        }
    }
    return true;
}

static const char* kTestCircuit = R"({
    "components": [
        {"id": 1, "typeName": "NUM_IN", "pos_x": -500, "pos_y": 0, "size_x": 90, "size_y": 140, "busWidth": 1,
         "driverLayout": [{"side": 2, "t": 0.8}, {"side": 2, "t": 0.6}, {"side": 2, "t": 0.4}, {"side": 2, "t": 0.2}],
         "receiverLayout": []},
        {"id": 2, "typeName": "NUM_IN", "pos_x": -500, "pos_y": 200, "size_x": 90, "size_y": 140, "busWidth": 1,
         "driverLayout": [{"side": 2, "t": 0.8}, {"side": 2, "t": 0.6}, {"side": 2, "t": 0.4}, {"side": 2, "t": 0.2}],
         "receiverLayout": []},
        {"id": 3, "typeName": "SW", "pos_x": -500, "pos_y": 400, "size_x": 90, "size_y": 56, "busWidth": 1,
         "driverLayout": [{"side": 2, "t": 0.5}], "receiverLayout": [], "state": false},
        {"id": 10, "typeName": "BUS_MERGE", "pos_x": -300, "pos_y": 0, "size_x": 90, "size_y": 140, "busWidth": 4,
         "driverLayout": [{"side": 2, "t": 0.8}, {"side": 2, "t": 0.6}, {"side": 2, "t": 0.4}, {"side": 2, "t": 0.2}],
         "receiverLayout": [{"side": 0, "t": 0.8}, {"side": 0, "t": 0.6}, {"side": 0, "t": 0.4}, {"side": 0, "t": 0.2}]},
        {"id": 11, "typeName": "BUS_MERGE", "pos_x": -300, "pos_y": 200, "size_x": 90, "size_y": 140, "busWidth": 4,
         "driverLayout": [{"side": 2, "t": 0.8}, {"side": 2, "t": 0.6}, {"side": 2, "t": 0.4}, {"side": 2, "t": 0.2}],
         "receiverLayout": [{"side": 0, "t": 0.8}, {"side": 0, "t": 0.6}, {"side": 0, "t": 0.4}, {"side": 0, "t": 0.2}]},
        {"id": 4, "typeName": "4-Bit Switch", "pos_x": 0, "pos_y": 100, "size_x": 100, "size_y": 200, "busWidth": 4,
         "driverLayout": [{"side": 2, "t": 0.8}, {"side": 2, "t": 0.6}, {"side": 2, "t": 0.4}, {"side": 2, "t": 0.2}],
         "receiverLayout": [
             {"side": 0, "t": 0.8}, {"side": 0, "t": 0.6}, {"side": 0, "t": 0.4}, {"side": 0, "t": 0.2},
             {"side": 0, "t": 0.8}, {"side": 0, "t": 0.6}, {"side": 0, "t": 0.4}, {"side": 0, "t": 0.2},
             {"side": 1, "t": 0.5}
         ]},
        {"id": 12, "typeName": "BUS_SPLIT", "pos_x": 200, "pos_y": 100, "size_x": 90, "size_y": 140, "busWidth": 4,
         "driverLayout": [{"side": 2, "t": 0.8}, {"side": 2, "t": 0.6}, {"side": 2, "t": 0.4}, {"side": 2, "t": 0.2}],
         "receiverLayout": [{"side": 0, "t": 0.8}, {"side": 0, "t": 0.6}, {"side": 0, "t": 0.4}, {"side": 0, "t": 0.2}]},
        {"id": 5, "typeName": "NUM_DISP", "pos_x": 400, "pos_y": 100, "size_x": 90, "size_y": 140, "busWidth": 1,
         "driverLayout": [], "receiverLayout": [
             {"side": 0, "t": 0.8}, {"side": 0, "t": 0.6}, {"side": 0, "t": 0.4}, {"side": 0, "t": 0.2}
         ]}
    ],
    "junctions": [],
    "wires": [
        {"id": 1, "busWidth": 1, "src": {"kind": 0, "compId": 1, "pinIdx": 0, "isDriver": true}, "dst": {"kind": 0, "compId": 10, "pinIdx": 0, "isDriver": false}, "waypoints": []},
        {"id": 2, "busWidth": 1, "src": {"kind": 0, "compId": 1, "pinIdx": 1, "isDriver": true}, "dst": {"kind": 0, "compId": 10, "pinIdx": 1, "isDriver": false}, "waypoints": []},
        {"id": 3, "busWidth": 1, "src": {"kind": 0, "compId": 1, "pinIdx": 2, "isDriver": true}, "dst": {"kind": 0, "compId": 10, "pinIdx": 2, "isDriver": false}, "waypoints": []},
        {"id": 4, "busWidth": 1, "src": {"kind": 0, "compId": 1, "pinIdx": 3, "isDriver": true}, "dst": {"kind": 0, "compId": 10, "pinIdx": 3, "isDriver": false}, "waypoints": []},
        {"id": 5, "busWidth": 1, "src": {"kind": 0, "compId": 2, "pinIdx": 0, "isDriver": true}, "dst": {"kind": 0, "compId": 11, "pinIdx": 0, "isDriver": false}, "waypoints": []},
        {"id": 6, "busWidth": 1, "src": {"kind": 0, "compId": 2, "pinIdx": 1, "isDriver": true}, "dst": {"kind": 0, "compId": 11, "pinIdx": 1, "isDriver": false}, "waypoints": []},
        {"id": 7, "busWidth": 1, "src": {"kind": 0, "compId": 2, "pinIdx": 2, "isDriver": true}, "dst": {"kind": 0, "compId": 11, "pinIdx": 2, "isDriver": false}, "waypoints": []},
        {"id": 8, "busWidth": 1, "src": {"kind": 0, "compId": 2, "pinIdx": 3, "isDriver": true}, "dst": {"kind": 0, "compId": 11, "pinIdx": 3, "isDriver": false}, "waypoints": []},
        {"id": 9, "busWidth": 4, "src": {"kind": 0, "compId": 10, "pinIdx": 0, "isDriver": true}, "dst": {"kind": 0, "compId": 4, "pinIdx": 0, "isDriver": false}, "waypoints": []},
        {"id": 10, "busWidth": 4, "src": {"kind": 0, "compId": 11, "pinIdx": 0, "isDriver": true}, "dst": {"kind": 0, "compId": 4, "pinIdx": 4, "isDriver": false}, "waypoints": []},
        {"id": 11, "busWidth": 1, "src": {"kind": 0, "compId": 3, "pinIdx": 0, "isDriver": true}, "dst": {"kind": 0, "compId": 4, "pinIdx": 8, "isDriver": false}, "waypoints": []},
        {"id": 12, "busWidth": 4, "src": {"kind": 0, "compId": 4, "pinIdx": 0, "isDriver": true}, "dst": {"kind": 0, "compId": 12, "pinIdx": 0, "isDriver": false}, "waypoints": []},
        {"id": 13, "busWidth": 1, "src": {"kind": 0, "compId": 12, "pinIdx": 0, "isDriver": true}, "dst": {"kind": 0, "compId": 5, "pinIdx": 0, "isDriver": false}, "waypoints": []},
        {"id": 14, "busWidth": 1, "src": {"kind": 0, "compId": 12, "pinIdx": 1, "isDriver": true}, "dst": {"kind": 0, "compId": 5, "pinIdx": 1, "isDriver": false}, "waypoints": []},
        {"id": 15, "busWidth": 1, "src": {"kind": 0, "compId": 12, "pinIdx": 2, "isDriver": true}, "dst": {"kind": 0, "compId": 5, "pinIdx": 2, "isDriver": false}, "waypoints": []},
        {"id": 16, "busWidth": 1, "src": {"kind": 0, "compId": 12, "pinIdx": 3, "isDriver": true}, "dst": {"kind": 0, "compId": 5, "pinIdx": 3, "isDriver": false}, "waypoints": []}
    ]
})";

int main()
{
    CustomComponentDef oneBit, fourBit;
    if (!loadCustomDef("1-Bit Switch.json", oneBit) || !loadCustomDef("4-Bit Switch.json", fourBit)) {
        std::cerr << "Failed to load custom component JSON files\n";
        return 1;
    }

    Simulator sim;
    Canvas canvas(&sim);
    canvas.customDefs[oneBit.typeName] = oneBit;
    canvas.customDefs[fourBit.typeName] = fourBit;
    canvas.deserialize(kTestCircuit, false);

    NumericInput* aIn = nullptr;
    NumericInput* bIn = nullptr;
    NumericDisplay* disp = nullptr;
    for (const auto& cv : canvas.getComps()) {
        if (cv.id == 1) aIn = dynamic_cast<NumericInput*>(cv.comp.get());
        if (cv.id == 2) bIn = dynamic_cast<NumericInput*>(cv.comp.get());
        if (cv.id == 5) disp = dynamic_cast<NumericDisplay*>(cv.comp.get());
    }
    if (!aIn || !bIn || !disp) {
        std::cerr << "Missing test components\n";
        return 1;
    }

    for (const auto& cv : canvas.getComps()) {
        if (cv.typeName == "4-Bit Switch") {
            if (auto* cc = dynamic_cast<CustomComponent*>(cv.comp.get())) {
                // Internal canvas should contain 11 components (incl. four 1-Bit Switches)
                json inner = json::parse(cc->getDef().canvasJson);
                int expected = static_cast<int>(inner["components"].size());
                // receivers: 4+4+1=9, drivers: 4
                if (cv.comp->numReceivers() != 9 || cv.comp->numDrivers() != 4) {
                    std::cerr << "4-Bit Switch pin map wrong: "
                              << cv.comp->numReceivers() << " receivers, "
                              << cv.comp->numDrivers() << " drivers (expected 9/4)\n";
                    return 1;
                }
                // Without nested customDefs, internal 1-Bit Switches are skipped and
                // the flattened circuit is missing ~4 subcomponents worth of logic.
                if (static_cast<int>(inner["components"].size()) != 11) {
                    std::cerr << "Unexpected internal component count in definition\n";
                    return 1;
                }
            } else {
                std::cerr << "4-Bit Switch is not a CustomComponent\n";
                return 1;
            }
        }
    }

    aIn->setValue(0xA);
    bIn->setValue(0x5);
    sim.settle(128);

    // S=0 (default): output should be A = 10
    if (disp->hasAmbiguity() || disp->getValue() != 0xA) {
        std::cerr << "Expected output 10 (A), got ";
        if (disp->hasAmbiguity()) std::cerr << "ambiguous/undefined";
        else std::cerr << disp->getValue();
        std::cerr << "\n";
        return 1;
    }

    // Toggle select to B
    for (const auto& cv : canvas.getComps()) {
        if (cv.id == 3) {
            if (auto* sw = dynamic_cast<Switch*>(cv.comp.get())) {
                sw->setOutput(true);
            }
        }
    }
    sim.settle(128);

    if (disp->hasAmbiguity() || disp->getValue() != 0x5) {
        std::cerr << "Expected output 5 (B), got ";
        if (disp->hasAmbiguity()) std::cerr << "ambiguous/undefined";
        else std::cerr << disp->getValue();
        std::cerr << "\n";
        return 1;
    }

    std::cout << "test_nested_custom: OK\n";
    return 0;
}
