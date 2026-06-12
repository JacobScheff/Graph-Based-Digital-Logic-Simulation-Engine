#include "Canvas.hpp"
#include "json.hpp"
#include "IO.hpp"
#include <iostream>

using json = nlohmann::json;

std::string Canvas::serialize() const
{
    json j;
    j["components"] = json::array();
    for (const auto& cv : comps) {
        json jc;
        jc["id"] = cv.id;
        jc["typeName"] = cv.typeName;
        jc["pos_x"] = cv.pos.x;
        jc["pos_y"] = cv.pos.y;
        jc["size_x"] = cv.size.x;
        jc["size_y"] = cv.size.y;
        jc["busWidth"] = cv.busWidth;
        
        if (cv.typeName == "PORT_IN") {
            if (auto* p = dynamic_cast<PortIn*>(cv.comp.get()))
                jc["label"] = p->label;
        } else if (cv.typeName == "PORT_OUT") {
            if (auto* p = dynamic_cast<PortOut*>(cv.comp.get()))
                jc["label"] = p->label;
        } else if (cv.typeName == "CLK") {
            if (auto* c = dynamic_cast<Clock*>(cv.comp.get()))
                jc["clockPeriod"] = c->getHalfPeriod();
        } else if (cv.typeName == "SW") {
            if (auto* s = dynamic_cast<Switch*>(cv.comp.get()))
                jc["state"] = s->isOn();
        } else if (cv.typeName == "BTN") {
            if (auto* b = dynamic_cast<Button*>(cv.comp.get()))
                jc["state"] = b->isPressed();
        }

        // Serialize PinLayout
        jc["receiverLayout"] = json::array();
        for (const auto& pl : cv.receiverLayout) {
            json jpl;
            jpl["side"] = pl.side;
            jpl["t"] = pl.t;
            jc["receiverLayout"].push_back(jpl);
        }
        
        jc["driverLayout"] = json::array();
        for (const auto& pl : cv.driverLayout) {
            json jpl;
            jpl["side"] = pl.side;
            jpl["t"] = pl.t;
            jc["driverLayout"].push_back(jpl);
        }

        j["components"].push_back(jc);
    }

    j["junctions"] = json::array();
    for (const auto& jv : junctions) {
        json jj;
        jj["id"] = jv.id;
        jj["pos_x"] = jv.pos.x;
        jj["pos_y"] = jv.pos.y;
        j["junctions"].push_back(jj);
    }

    j["wires"] = json::array();
    for (const auto& wv : wires) {
        json jw;
        jw["id"] = wv.id;
        jw["busWidth"] = wv.busWidth;
        
        auto serializeEndpoint = [](const Endpoint& ep) -> json {
            json je;
            je["kind"] = static_cast<int>(ep.kind);
            je["compId"] = ep.compId;
            je["pinIdx"] = ep.pinIdx;
            je["isDriver"] = ep.isDriver;
            je["railIsVdd"] = ep.railIsVdd;
            je["railX"] = ep.railX;
            je["junctionId"] = ep.junctionId;
            return je;
        };
        
        jw["src"] = serializeEndpoint(wv.src);
        jw["dst"] = serializeEndpoint(wv.dst);

        // Serialize waypoints
        jw["waypoints"] = json::array();
        for (const auto& wp : wv.waypoints) {
            json jwp;
            jwp["x"] = wp.x;
            jwp["y"] = wp.y;
            jw["waypoints"].push_back(jwp);
        }

        j["wires"].push_back(jw);
    }

    return j.dump(4);
}

void Canvas::deserialize(const std::string& data, bool ignoreInputStates)
{
    if (data.empty()) return;
    
    // Clear everything
    clearSelection();
    comps.clear();
    junctions.clear();
    wires.clear();
    if (sim) {
        sim->stop();
    }
    
    try {
        json j = json::parse(data);
        
        int maxCompId = 0;
        int maxJunctionId = 0;
        int maxWireId = 0;

        // Load components
        if (j.contains("components")) {
            for (const auto& jc : j["components"]) {
                ComponentView cv;
                cv.id = jc["id"].get<int>();
                cv.typeName = jc["typeName"].get<std::string>();
                cv.pos = { jc["pos_x"].get<float>(), jc["pos_y"].get<float>() };
                cv.size = { jc["size_x"].get<float>(), jc["size_y"].get<float>() };
                cv.busWidth = jc.value("busWidth", 1);
                
                cv.comp = makeComponent(cv.typeName, cv.busWidth);
                if (!cv.comp) continue;
                
                if (cv.typeName == "PORT_IN") {
                    if (auto* p = dynamic_cast<PortIn*>(cv.comp.get()))
                        p->label = jc.value("label", "in");
                } else if (cv.typeName == "PORT_OUT") {
                    if (auto* p = dynamic_cast<PortOut*>(cv.comp.get()))
                        p->label = jc.value("label", "out");
                } else if (cv.typeName == "CLK") {
                    if (auto* c = dynamic_cast<Clock*>(cv.comp.get()))
                        c->setHalfPeriod(jc.value("clockPeriod", 10));
                } else if (!ignoreInputStates) {
                    if (cv.typeName == "SW") {
                        if (auto* s = dynamic_cast<Switch*>(cv.comp.get())) {
                            bool isHigh = jc.value("state", false);
                            s->setOutput(isHigh);
                        }
                    } else if (cv.typeName == "BTN") {
                        if (auto* b = dynamic_cast<Button*>(cv.comp.get())) {
                            bool isHigh = jc.value("state", false);
                            if (isHigh) {
                                b->press();
                            } else {
                                b->release();
                            }
                        }
                    }
                }
                
                // Load PinLayout or initialize defaults
                if (jc.contains("receiverLayout")) {
                    for (const auto& jpl : jc["receiverLayout"]) {
                        PinLayout pl;
                        pl.side = jpl.value("side", 0);
                        pl.t = jpl.value("t", 0.5f);
                        cv.receiverLayout.push_back(pl);
                    }
                }
                if (jc.contains("driverLayout")) {
                    for (const auto& jpl : jc["driverLayout"]) {
                        PinLayout pl;
                        pl.side = jpl.value("side", 2);
                        pl.t = jpl.value("t", 0.5f);
                        cv.driverLayout.push_back(pl);
                    }
                }
                
                // If layouts were missing (old save), initialize defaults
                if (cv.receiverLayout.empty() || cv.driverLayout.empty()) {
                    initPinLayouts(cv);
                }
                
                if (sim) sim->registerComponent(cv.comp.get());
                if (cv.typeName == "CLK" && sim) {
                    if (auto* clk = dynamic_cast<Clock*>(cv.comp.get()))
                        sim->registerClock(clk);
                }
                
                comps.push_back(std::move(cv));
                if (cv.id > maxCompId) maxCompId = cv.id;
            }
        }
        
        // Load junctions
        if (j.contains("junctions")) {
            for (const auto& jj : j["junctions"]) {
                JunctionView jv;
                jv.id = jj["id"].get<int>();
                jv.pos = { jj["pos_x"].get<float>(), jj["pos_y"].get<float>() };
                junctions.push_back(jv);
                if (jv.id > maxJunctionId) maxJunctionId = jv.id;
            }
        }
        
        // Update ID counters so future placements don't collide
        nextId = maxCompId + 1;
        nextJunctionId = maxJunctionId + 1;
        
        // Load wires
        if (j.contains("wires")) {
            for (const auto& jw : j["wires"]) {
                auto deserializeEndpoint = [](const json& je) -> Endpoint {
                    Endpoint ep;
                    ep.kind = static_cast<EndpointKind>(je.value("kind", 0));
                    ep.compId = je.value("compId", -1);
                    ep.pinIdx = je.value("pinIdx", -1);
                    ep.isDriver = je.value("isDriver", false);
                    ep.railIsVdd = je.value("railIsVdd", true);
                    ep.railX = je.value("railX", 0.0f);
                    ep.junctionId = je.value("junctionId", -1);
                    return ep;
                };
                
                Endpoint src = deserializeEndpoint(jw["src"]);
                Endpoint dst = deserializeEndpoint(jw["dst"]);
                int busWidth = jw.value("busWidth", 1);
                
                // Add waypoints if they exist, before completeWire is called.
                // completeWire creates the wire object and puts it at the back of `wires`.
                completeWire(src, dst, busWidth);
                
                if (!wires.empty()) {
                    WireView& wv = wires.back();
                    if (jw.contains("waypoints")) {
                        for (const auto& jwp : jw["waypoints"]) {
                            wv.waypoints.push_back({jwp.value("x", 0.f), jwp.value("y", 0.f)});
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Deserialization error: " << e.what() << "\n";
    }
}
