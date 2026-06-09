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
        j["wires"].push_back(jw);
    }

    return j.dump(4);
}

void Canvas::deserialize(const std::string& data)
{
    if (data.empty()) return;
    
    // Clear everything
    clearSelection();
    comps.clear();
    junctions.clear();
    wires.clear();
    if (sim) {
        sim->stop();
        // The simulator handles its own nets, but we need a clean slate.
        // Easiest is to create a new Simulator, but since we just use sim->createNet() in completeWire,
        // we should probably clear simulator nets if possible. 
        // For now, Simulator will accumulate orphan nets. To fix, one should add Simulator::clear().
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
                }
                
                if (sim) sim->registerComponent(cv.comp.get());
                
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
                
                // Using completeWire will automatically create the Net and wire object!
                // Because completeWire pushes to `wires` array, the wire IDs will be assigned from `nextWireId`.
                // We will ignore the saved wire ID to guarantee consistency.
                completeWire(src, dst, busWidth);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Deserialization error: " << e.what() << "\n";
    }
}
