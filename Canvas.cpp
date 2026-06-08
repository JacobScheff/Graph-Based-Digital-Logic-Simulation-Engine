#include "Canvas.hpp"
#include "Gates.hpp"
#include "IO.hpp"
#include "Net.hpp"
#include "Pin.hpp"
#include "PowerRails.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

ImU32 Canvas::stateColor(State s) const
{
    if (sim) return stateColorRail(s, *sim);
    switch (s) {
        case State::LOW:       return IM_COL32( 33, 150, 243, 255);
        case State::HIGH:      return IM_COL32( 76, 175,  80, 255);
        case State::FLOATING:  return IM_COL32(120, 120, 120, 255);
        case State::UNDEFINED: return IM_COL32(244,  67,  54, 255);
    }
    return IM_COL32(255,255,255,255);
}

ImVec2 Canvas::railEndpointWorld(bool isVdd, float worldX,
                                 ImVec2 origin, ImVec2 canvasSize) const
{
    float sy = isVdd ? origin.y + RAIL_BAND * 0.5f
                     : origin.y + canvasSize.y - RAIL_BAND * 0.5f;
    float worldY = (sy - origin.y) / zoom + pan.y;
    return { worldX, worldY };
}

Canvas::Canvas(Simulator* sim) : sim(sim) {}

Canvas::~Canvas()
{
    for (auto& cv : comps)
        sim->unregisterComponent(cv.comp.get());
}

ImVec2 Canvas::w2s(ImVec2 w, ImVec2 origin) const
{
    return { origin.x + (w.x - pan.x) * zoom,
             origin.y + (w.y - pan.y) * zoom };
}

ImVec2 Canvas::s2w(ImVec2 s, ImVec2 origin) const
{
    return { (s.x - origin.x) / zoom + pan.x,
             (s.y - origin.y) / zoom + pan.y };
}

ImVec2 Canvas::snapToGrid(ImVec2 w) const
{
    return { std::round(w.x / SNAP) * SNAP,
             std::round(w.y / SNAP) * SNAP };
}

float Canvas::railScreenY(bool vdd, ImVec2 origin) const
{
    return vdd ? origin.y + RAIL_BAND * 0.5f : origin.y; // set by caller with size
}

ImVec2 Canvas::railTapWorldX(float worldX) const
{
    return { std::round(worldX / SNAP) * SNAP, 0.f };
}

ImVec2 Canvas::driverPos(const ComponentView& cv, int idx) const
{
    int n = cv.comp->numDrivers();
    if (n == 0) return cv.pos;
    float step = cv.size.y / float(n + 1);
    return { cv.pos.x + cv.size.x + PIN_LEN,
             cv.pos.y + step * float(idx + 1) };
}

static bool hasVddGndInputs(const std::string& typeName)
{
    return typeName == "NOT" || typeName == "BUF" ||
           typeName == "AND" || typeName == "NAND" ||
           typeName == "OR"  || typeName == "NOR"  ||
           typeName == "XOR" || typeName == "XNOR" ||
           typeName == "CLK";
}

ImVec2 Canvas::receiverPos(const ComponentView& cv, int idx) const
{
    int n = cv.comp->numReceivers();
    if (n == 0) return cv.pos;

    if (hasVddGndInputs(cv.typeName)) {
        if (idx == n - 2) {
            // VDD input: Top border middle
            return { cv.pos.x + cv.size.x * 0.5f, cv.pos.y - PIN_LEN };
        }
        if (idx == n - 1) {
            // GND input: Bottom border middle
            return { cv.pos.x + cv.size.x * 0.5f, cv.pos.y + cv.size.y + PIN_LEN };
        }
        // Regular inputs on the left: total of n - 2 inputs
        int numRegular = n - 2;
        if (numRegular == 0) return cv.pos;
        float step = cv.size.y / float(numRegular + 1);
        return { cv.pos.x - PIN_LEN,
                 cv.pos.y + step * float(idx + 1) };
    }

    float step = cv.size.y / float(n + 1);
    return { cv.pos.x - PIN_LEN,
             cv.pos.y + step * float(idx + 1) };
}

ImVec2 Canvas::receiverEdgePos(const ComponentView& cv, int idx) const
{
    int n = cv.comp->numReceivers();
    if (n == 0) return cv.pos;

    if (hasVddGndInputs(cv.typeName)) {
        if (idx == n - 2) {
            return { cv.pos.x + cv.size.x * 0.5f, cv.pos.y };
        }
        if (idx == n - 1) {
            return { cv.pos.x + cv.size.x * 0.5f, cv.pos.y + cv.size.y };
        }
    }
    return { cv.pos.x, receiverPos(cv, idx).y };
}

ImVec2 Canvas::busDriverPos(const ComponentView& cv) const
{
    return { cv.pos.x + cv.size.x + PIN_LEN, cv.pos.y + cv.size.y * 0.5f };
}

ImVec2 Canvas::busReceiverPos(const ComponentView& cv) const
{
    return { cv.pos.x - PIN_LEN, cv.pos.y + cv.size.y * 0.5f };
}

ImVec2 Canvas::endpointPos(const Endpoint& ep, ImVec2 origin, ImVec2 canvasSize) const
{
    switch (ep.kind) {
        case EndpointKind::Component: {
            const auto* cv = findComp(ep.compId);
            if (!cv) return {0, 0};
            if (isBusComponent(cv->typeName) && ep.pinIdx == 0) {
                if (cv->typeName == "BUS_MERGE" && ep.isDriver)
                    return busDriverPos(*cv);
                if (cv->typeName == "BUS_SPLIT" && !ep.isDriver)
                    return busReceiverPos(*cv);
            }
            if (ep.isDriver) {
                if (ep.pinIdx >= 0 && ep.pinIdx < cv->comp->numDrivers())
                    return driverPos(*cv, ep.pinIdx);
            } else {
                if (ep.pinIdx >= 0 && ep.pinIdx < cv->comp->numReceivers())
                    return receiverPos(*cv, ep.pinIdx);
            }
            return cv->pos;
        }
        case EndpointKind::Rail:
            return railEndpointWorld(ep.railIsVdd, ep.railX, origin, canvasSize);
        case EndpointKind::Junction: {
            const auto* j = findJunction(ep.junctionId);
            return j ? j->pos : ImVec2{0, 0};
        }
    }
    return {0, 0};
}

bool Canvas::isBusComponent(const std::string& type) const
{
    return type == "BUS_MERGE" || type == "BUS_SPLIT";
}

int Canvas::componentBusWidth(const ComponentView& cv) const
{
    if (isBusComponent(cv.typeName))
        return cv.busWidth;
    if (cv.typeName == "NUM_IN" || cv.typeName == "NUM_DISP")
        return 4;
    return 1;
}

std::unique_ptr<Component> Canvas::makeComponent(const std::string& type, int busWidth)
{
    if (type == "NOT")      return std::make_unique<NotGate>();
    if (type == "BUF")      return std::make_unique<Buffer>();
    if (type == "AND")      return std::make_unique<AndGate>();
    if (type == "NAND")     return std::make_unique<NandGate>();
    if (type == "OR")       return std::make_unique<OrGate>();
    if (type == "NOR")      return std::make_unique<NorGate>();
    if (type == "XOR")      return std::make_unique<XorGate>();
    if (type == "XNOR")     return std::make_unique<XnorGate>();
    if (type == "SW")       return std::make_unique<Switch>();
    if (type == "BTN")      return std::make_unique<Button>();
    if (type == "CLK")      return std::make_unique<Clock>(10);
    if (type == "LED")      return std::make_unique<LED>();
    if (type == "NUM_IN")   return std::make_unique<NumericInput>();
    if (type == "NUM_DISP") return std::make_unique<NumericDisplay>();
    if (type == "JUNCTION") return std::make_unique<Junction>();
    if (type == "BUS_MERGE") return std::make_unique<BusMerge>(busWidth);
    if (type == "BUS_SPLIT") return std::make_unique<BusSplit>(busWidth);
    return nullptr;
}

ImVec2 Canvas::getComponentSize(const std::string& type, int busWidth)
{
    int numReceivers = 0;
    int numDrivers = 0;
    
    if (type == "NOT" || type == "BUF") {
        numReceivers = 3;
        numDrivers = 1;
    } else if (type == "AND" || type == "NAND" || type == "OR" || type == "NOR" || type == "XOR" || type == "XNOR") {
        numReceivers = 4;
        numDrivers = 1;
    } else if (type == "SW" || type == "BTN") {
        numReceivers = 1;
        numDrivers = 1;
    } else if (type == "CLK") {
        numReceivers = 2;
        numDrivers = 1;
    } else if (type == "LED") {
        numReceivers = 1;
        numDrivers = 0;
    } else if (type == "NUM_IN") {
        numReceivers = 0;
        numDrivers = 4;
    } else if (type == "NUM_DISP") {
        numReceivers = 4;
        numDrivers = 0;
    } else if (type == "JUNCTION") {
        numReceivers = 1;
        numDrivers = 1;
    } else if (type == "BUS_MERGE" || type == "BUS_SPLIT") {
        numReceivers = busWidth;
        numDrivers = busWidth;
    }
    
    int maxPins = std::max(numReceivers, numDrivers);
    float h = std::max(50.f, float(maxPins + 1) * PIN_SPACE);
    if (type == "BUS_MERGE" || type == "BUS_SPLIT") {
        h = std::max(h, float(busWidth + 1) * PIN_SPACE);
    }
    return { COMP_W, h };
}

Canvas::ComponentView Canvas::makeView(const std::string& type, ImVec2 worldPos, int busWidth)
{
    ComponentView cv;
    cv.id       = nextId++;
    cv.typeName = type;
    cv.busWidth = busWidth;
    cv.comp     = makeComponent(type, busWidth);
    cv.comp->setBusWidth(busWidth);
    cv.pos      = worldPos;
    cv.size     = getComponentSize(type, busWidth);

    sim->registerComponent(cv.comp.get());

    if (auto* clk = dynamic_cast<Clock*>(cv.comp.get()))
        sim->registerClock(clk);

    return cv;
}

void Canvas::placeAt(const std::string& type, ImVec2 worldPos, int busWidth)
{
    worldPos = snapToGrid(worldPos);
    worldPos.x -= COMP_W / 2.f;
    worldPos.y -= 30.f;

    comps.push_back(makeView(type, worldPos, busWidth));
    selectedId = comps.back().id;
}

void Canvas::beginPlacement(const std::string& typeName, int busWidth)
{
    mode            = Mode::Placing;
    pendingType     = typeName;
    pendingBusWidth = busWidth;
    selectedId      = -1;
}

void Canvas::completeWireSingle(Driver* drv, Receiver* rcv,
                                const Endpoint& srcIn, const Endpoint& dstIn)
{
    Endpoint src = srcIn;
    Endpoint dst = dstIn;
    if (src.kind == EndpointKind::Rail && dst.kind != EndpointKind::Rail) {
        src.railX = endpointPos(dst, {0,0}, {0,0}).x;
    } else if (dst.kind == EndpointKind::Rail && src.kind != EndpointKind::Rail) {
        dst.railX = endpointPos(src, {0,0}, {0,0}).x;
    }

    if (!drv || !rcv || rcv->isConnected()) return;

    Net* net = drv->isConnected() ? drv->getNet() : sim->connectDriver(drv);
    if (!net) return;
    if (!sim->canConnect(drv, rcv, net)) return;

    sim->connectReceiver(rcv, net);

    WireView wv;
    wv.id       = nextWireId++;
    wv.busWidth = 1;
    wv.net      = net;
    wv.src      = src;
    wv.dst      = dst;
    wires.push_back(wv);
}

void Canvas::completeWire(const Endpoint& srcIn, const Endpoint& dstIn, int busWidth)
{
    Endpoint src = srcIn;
    Endpoint dst = dstIn;

    if (src.kind == EndpointKind::Rail && dst.kind != EndpointKind::Rail) {
        src.railX = endpointPos(dst, {0,0}, {0,0}).x;
    } else if (dst.kind == EndpointKind::Rail && src.kind != EndpointKind::Rail) {
        dst.railX = endpointPos(src, {0,0}, {0,0}).x;
    }

    if (src.kind == dst.kind && src.compId == dst.compId && src.pinIdx == dst.pinIdx &&
        src.junctionId == dst.junctionId && src.railIsVdd == dst.railIsVdd) return;

    if (src.kind == EndpointKind::Rail && dst.kind == EndpointKind::Rail) return;

    if (busWidth <= 1) {
        Driver*   drv = nullptr;
        Receiver* rcv = nullptr;

        if (src.kind == EndpointKind::Component) {
            auto* cv = findComp(src.compId);
            if (cv && src.pinIdx >= 0 && src.pinIdx < cv->comp->numDrivers())
                drv = cv->comp->getDriver(src.pinIdx);
        }
        if (dst.kind == EndpointKind::Component) {
            auto* cv = findComp(dst.compId);
            if (cv && dst.pinIdx >= 0 && dst.pinIdx < cv->comp->numReceivers())
                rcv = cv->comp->getReceiver(dst.pinIdx);
        }

        Net* srcNet = nullptr;
        Net* dstNet = nullptr;
        if (src.kind == EndpointKind::Rail)
            srcNet = src.railIsVdd ? sim->getVddNet() : sim->getGndNet();
        else if (src.kind == EndpointKind::Junction) {
            if (auto* j = findJunction(src.junctionId)) srcNet = j->net;
        } else if (drv) {
            srcNet = drv->getNet();
        }

        if (dst.kind == EndpointKind::Rail)
            dstNet = dst.railIsVdd ? sim->getVddNet() : sim->getGndNet();
        else if (dst.kind == EndpointKind::Junction) {
            if (auto* j = findJunction(dst.junctionId)) dstNet = j->net;
        } else if (rcv) {
            dstNet = rcv->getNet();
        }

        Net* wireNet = nullptr;
        if (src.kind == EndpointKind::Rail) wireNet = srcNet;
        else if (dst.kind == EndpointKind::Rail) wireNet = dstNet;
        else if (srcNet) wireNet = srcNet;
        else if (dstNet) wireNet = dstNet;

        if (!wireNet) {
            if (drv) {
                wireNet = drv->isConnected() ? drv->getNet() : sim->connectDriver(drv);
            } else {
                wireNet = sim->createNet();
            }
        }

        if (!wireNet) return;

        if (drv && !drv->isConnected()) {
            sim->connectDriver(drv, wireNet);
        }
        if (rcv && !rcv->isConnected()) {
            if (!sim->canConnect(nullptr, rcv, wireNet)) return;
            sim->connectReceiver(rcv, wireNet);
        }

        if (src.kind == EndpointKind::Junction) {
            if (auto* j = findJunction(src.junctionId)) j->net = wireNet;
        }
        if (dst.kind == EndpointKind::Junction) {
            if (auto* j = findJunction(dst.junctionId)) j->net = wireNet;
        }

        WireView wv;
        wv.id       = nextWireId++;
        wv.busWidth = 1;
        wv.net      = wireNet;
        wv.src      = src;
        wv.dst      = dst;
        wires.push_back(wv);
        sim->settle(64);
        return;
    }

    // Multi-bit bus wiring
    ComponentView* srcCv = src.kind == EndpointKind::Component ? findComp(src.compId) : nullptr;
    ComponentView* dstCv = dst.kind == EndpointKind::Component ? findComp(dst.compId) : nullptr;
    if (!srcCv || !dstCv) return;

    WireView wv;
    wv.id       = nextWireId++;
    wv.busWidth = busWidth;
    wv.src      = src;
    wv.dst      = dst;

    for (int i = 0; i < busWidth; ++i) {
        Driver*   drv = srcCv->comp->getDriver(i);
        Receiver* rcv = dstCv->comp->getReceiver(i);
        if (!drv || !rcv || rcv->isConnected()) continue;

        Net* net = drv->isConnected() ? drv->getNet() : sim->connectDriver(drv);
        if (!net || !sim->canConnect(drv, rcv, net)) continue;
        sim->connectReceiver(rcv, net);
        wv.busNets.push_back(net);
    }

    if (!wv.busNets.empty()) {
        wv.net = wv.busNets[0];
        wires.push_back(wv);
    }
    sim->settle(64);
}

void Canvas::removeWiresOf(int compId)
{
    std::vector<WireView> remaining;
    for (auto& wv : wires) {
        bool touches = (wv.src.kind == EndpointKind::Component && wv.src.compId == compId)
                    || (wv.dst.kind == EndpointKind::Component && wv.dst.compId == compId);
        if (touches) {
            if (wv.src.kind == EndpointKind::Component && wv.src.compId == compId) {
                if (auto* cv = findComp(wv.src.compId)) {
                    if (wv.busWidth <= 1) {
                        if (auto* d = cv->comp->getDriver(wv.src.pinIdx))
                            sim->disconnectDriver(d);
                    } else {
                        for (int i = 0; i < wv.busWidth; ++i)
                            sim->disconnectDriver(cv->comp->getDriver(i));
                    }
                }
            }
            if (wv.dst.kind == EndpointKind::Component && wv.dst.compId == compId) {
                if (auto* cv = findComp(wv.dst.compId)) {
                    if (wv.busWidth <= 1) {
                        sim->disconnectReceiver(cv->comp->getReceiver(wv.dst.pinIdx));
                    } else {
                        for (int i = 0; i < wv.busWidth; ++i)
                            sim->disconnectReceiver(cv->comp->getReceiver(i));
                    }
                }
            }
            if (wv.net && wv.net != sim->getVddNet() && wv.net != sim->getGndNet()) {
                if (wv.net->getDrivers().empty() && wv.net->getReceivers().empty()) {
                    Net* netToDelete = wv.net;
                    sim->removeNet(netToDelete);
                    for (auto& w : wires) {
                        if (w.net == netToDelete) w.net = nullptr;
                        for (auto& bn : w.busNets) {
                            if (bn == netToDelete) bn = nullptr;
                        }
                    }
                    for (auto& j : junctions) {
                        if (j.net == netToDelete) j.net = nullptr;
                    }
                }
            }
            for (Net* bn : wv.busNets) {
                if (bn && bn != sim->getVddNet() && bn != sim->getGndNet()
                    && bn->getDrivers().empty() && bn->getReceivers().empty()) {
                    sim->removeNet(bn);
                    for (auto& w : wires) {
                        if (w.net == bn) w.net = nullptr;
                        for (auto& bnn : w.busNets) {
                            if (bnn == bn) bnn = nullptr;
                        }
                    }
                    for (auto& j : junctions) {
                        if (j.net == bn) j.net = nullptr;
                    }
                }
            }
        } else {
            remaining.push_back(wv);
        }
    }
    wires = std::move(remaining);
    cleanupDanglingJunctions();
}

void Canvas::removeWire(int wireId)
{
    auto it = std::find_if(wires.begin(), wires.end(),
                           [&](const WireView& w){ return w.id == wireId; });
    if (it != wires.end()) {
        const auto& wv = *it;
        if (wv.src.kind == EndpointKind::Component) {
            if (auto* cv = findComp(wv.src.compId)) {
                if (wv.busWidth <= 1) {
                    if (auto* d = cv->comp->getDriver(wv.src.pinIdx))
                        sim->disconnectDriver(d);
                } else {
                    for (int i = 0; i < wv.busWidth; ++i)
                        sim->disconnectDriver(cv->comp->getDriver(i));
                }
            }
        }
        if (wv.dst.kind == EndpointKind::Component) {
            if (auto* cv = findComp(wv.dst.compId)) {
                if (wv.busWidth <= 1) {
                    sim->disconnectReceiver(cv->comp->getReceiver(wv.dst.pinIdx));
                } else {
                    for (int i = 0; i < wv.busWidth; ++i)
                        sim->disconnectReceiver(cv->comp->getReceiver(i));
                }
            }
        }
        if (wv.net && wv.net != sim->getVddNet() && wv.net != sim->getGndNet()) {
            if (wv.net->getDrivers().empty() && wv.net->getReceivers().empty()) {
                Net* netToDelete = wv.net;
                sim->removeNet(netToDelete);
                for (auto& w : wires) {
                    if (w.net == netToDelete) w.net = nullptr;
                    for (auto& bn : w.busNets) {
                        if (bn == netToDelete) bn = nullptr;
                    }
                }
                for (auto& j : junctions) {
                    if (j.net == netToDelete) j.net = nullptr;
                }
            }
        }
        for (Net* bn : wv.busNets) {
            if (bn && bn != sim->getVddNet() && bn != sim->getGndNet()
                && bn->getDrivers().empty() && bn->getReceivers().empty()) {
                sim->removeNet(bn);
                for (auto& w : wires) {
                    if (w.net == bn) w.net = nullptr;
                    for (auto& bnn : w.busNets) {
                        if (bnn == bn) bnn = nullptr;
                    }
                }
                for (auto& j : junctions) {
                    if (j.net == bn) j.net = nullptr;
                }
            }
        }
        wires.erase(it);
    }
}

void Canvas::removeJunction(int junctionId)
{
    junctions.erase(std::remove_if(junctions.begin(), junctions.end(),
        [&](const JunctionView& j){ return j.id == junctionId; }), junctions.end());
}

void Canvas::cleanupDanglingJunctions()
{
    std::vector<JunctionView> remaining;
    for (const auto& j : junctions) {
        bool referenced = false;
        for (const auto& wv : wires) {
            if ((wv.src.kind == EndpointKind::Junction && wv.src.junctionId == j.id) ||
                (wv.dst.kind == EndpointKind::Junction && wv.dst.junctionId == j.id)) {
                referenced = true;
                break;
            }
        }
        if (referenced) {
            remaining.push_back(j);
        }
    }
    junctions = std::move(remaining);
}

void Canvas::insertJunctionOnWire(int wireId, ImVec2 worldPos)
{
    for (const auto& wv : wires) {
        if (wv.id != wireId || !wv.net) continue;
        JunctionView jv;
        jv.id  = nextJunctionId++;
        jv.net = wv.net;
        jv.pos = snapToGrid(worldPos);
        junctions.push_back(jv);
        return;
    }
}

void Canvas::deleteSelected()
{
    std::vector<int> compsToDelete;
    std::vector<int> junctionsToDelete;
    std::vector<int> wiresToDelete;
    for (const auto& cv : comps) {
        if (cv.selected) compsToDelete.push_back(cv.id);
    }
    for (const auto& j : junctions) {
        if (j.selected) junctionsToDelete.push_back(j.id);
    }
    for (const auto& w : wires) {
        if (w.selected) wiresToDelete.push_back(w.id);
    }

    if (compsToDelete.empty() && selectedId >= 0) {
        compsToDelete.push_back(selectedId);
    }

    for (int wId : wiresToDelete) {
        removeWire(wId);
    }

    for (int compId : compsToDelete) {
        removeWiresOf(compId);
    }

    for (int compId : compsToDelete) {
        auto it = std::find_if(comps.begin(), comps.end(),
                               [&](const ComponentView& cv){ return cv.id == compId; });
        if (it != comps.end()) {
            if (auto* clk = dynamic_cast<Clock*>(it->comp.get()))
                sim->unregisterClock(clk);
            else
                sim->unregisterComponent(it->comp.get());
            comps.erase(it);
        }
    }

    for (int jId : junctionsToDelete) {
        removeJunction(jId);
    }

    if (!junctionsToDelete.empty()) {
        std::vector<WireView> remainingWires;
        for (auto& wv : wires) {
            bool touchesDeletedJunction = 
                (wv.src.kind == EndpointKind::Junction && std::find(junctionsToDelete.begin(), junctionsToDelete.end(), wv.src.junctionId) != junctionsToDelete.end()) ||
                (wv.dst.kind == EndpointKind::Junction && std::find(junctionsToDelete.begin(), junctionsToDelete.end(), wv.dst.junctionId) != junctionsToDelete.end());
            if (touchesDeletedJunction) {
                if (wv.src.kind == EndpointKind::Component) {
                    if (auto* cv = findComp(wv.src.compId)) {
                        if (wv.busWidth <= 1) {
                            if (auto* d = cv->comp->getDriver(wv.src.pinIdx)) sim->disconnectDriver(d);
                        } else {
                            for (int i = 0; i < wv.busWidth; ++i) sim->disconnectDriver(cv->comp->getDriver(i));
                        }
                    }
                }
                if (wv.dst.kind == EndpointKind::Component) {
                    if (auto* cv = findComp(wv.dst.compId)) {
                        if (wv.busWidth <= 1) {
                            sim->disconnectReceiver(cv->comp->getReceiver(wv.dst.pinIdx));
                        } else {
                            for (int i = 0; i < wv.busWidth; ++i) sim->disconnectReceiver(cv->comp->getReceiver(i));
                        }
                    }
                }
                if (wv.net && wv.net != sim->getVddNet() && wv.net != sim->getGndNet()) {
                    if (wv.net->getDrivers().empty() && wv.net->getReceivers().empty()) {
                        Net* netToDelete = wv.net;
                        sim->removeNet(netToDelete);
                        for (auto& w : wires) {
                            if (w.net == netToDelete) w.net = nullptr;
                            for (auto& bn : w.busNets) {
                                if (bn == netToDelete) bn = nullptr;
                            }
                        }
                        for (auto& j : junctions) {
                            if (j.net == netToDelete) j.net = nullptr;
                        }
                    }
                }
            } else {
                remainingWires.push_back(wv);
            }
        }
        wires = std::move(remainingWires);
    }

    cleanupDanglingJunctions();
    selectedId = -1;
    sim->settle();
}

Canvas::ComponentView* Canvas::findComp(int id)
{
    for (auto& cv : comps)
        if (cv.id == id) return &cv;
    return nullptr;
}

const Canvas::ComponentView* Canvas::findComp(int id) const
{
    for (const auto& cv : comps)
        if (cv.id == id) return &cv;
    return nullptr;
}

Canvas::JunctionView* Canvas::findJunction(int id)
{
    for (auto& j : junctions)
        if (j.id == id) return &j;
    return nullptr;
}

const Canvas::JunctionView* Canvas::findJunction(int id) const
{
    for (const auto& j : junctions)
        if (j.id == id) return &j;
    return nullptr;
}

Component* Canvas::getSelectedComponent() const
{
    const auto* cv = findComp(selectedId);
    return cv ? cv->comp.get() : nullptr;
}

const std::string& Canvas::getSelectedTypeName() const
{
    static std::string empty;
    const auto* cv = findComp(selectedId);
    return cv ? cv->typeName : empty;
}

bool Canvas::hasSelection() const
{
    for (const auto& cv : comps) if (cv.selected) return true;
    for (const auto& j : junctions) if (j.selected) return true;
    for (const auto& w : wires) if (w.selected) return true;
    return selectedId >= 0;
}

int Canvas::getSelectedComponentCount() const
{
    int count = 0;
    for (const auto& cv : comps) {
        if (cv.selected) count++;
    }
    if (count == 0 && selectedId >= 0) count = 1;
    return count;
}

int Canvas::getSelectedWireCount() const
{
    int count = 0;
    for (const auto& w : wires) {
        if (w.selected) count++;
    }
    return count;
}

int Canvas::getSelectedJunctionCount() const
{
    int count = 0;
    for (const auto& j : junctions) {
        if (j.selected) count++;
    }
    return count;
}

void Canvas::clearSelection()
{
    for (auto& cv : comps) cv.selected = false;
    for (auto& j : junctions) j.selected = false;
    for (auto& w : wires) w.selected = false;
    selectedId = -1;
}

int Canvas::hitComp(ImVec2 wp) const
{
    for (auto it = comps.rbegin(); it != comps.rend(); ++it) {
        const auto& cv = *it;
        if (wp.x >= cv.pos.x && wp.x <= cv.pos.x + cv.size.x &&
            wp.y >= cv.pos.y && wp.y <= cv.pos.y + cv.size.y)
            return cv.id;
    }
    return -1;
}

int Canvas::hitDriverPin(ImVec2 wp, int& outId, bool busSide) const
{
    for (const auto& cv : comps) {
        if (busSide && isBusComponent(cv.typeName)) {
            ImVec2 p = busDriverPos(cv);
            float dx = wp.x - p.x, dy = wp.y - p.y;
            if (dx*dx + dy*dy <= (PIN_RAD+6)*(PIN_RAD+6)) {
                outId = cv.id;
                return 0;
            }
        }
        if (!busSide && cv.typeName == "BUS_MERGE") continue;
        for (int i = 0; i < cv.comp->numDrivers(); ++i) {
            ImVec2 p = driverPos(cv, i);
            float dx = wp.x - p.x, dy = wp.y - p.y;
            if (dx*dx + dy*dy <= (PIN_RAD+4)*(PIN_RAD+4)) {
                outId = cv.id;
                return i;
            }
        }
    }
    outId = -1;
    return -1;
}

int Canvas::hitReceiverPin(ImVec2 wp, int& outId, bool busSide) const
{
    for (const auto& cv : comps) {
        if (busSide && isBusComponent(cv.typeName)) {
            ImVec2 p = busReceiverPos(cv);
            float dx = wp.x - p.x, dy = wp.y - p.y;
            if (dx*dx + dy*dy <= (PIN_RAD+6)*(PIN_RAD+6)) {
                outId = cv.id;
                return 0;
            }
        }
        if (!busSide && cv.typeName == "BUS_SPLIT") continue;
        for (int i = 0; i < cv.comp->numReceivers(); ++i) {
            ImVec2 p = receiverPos(cv, i);
            float dx = wp.x - p.x, dy = wp.y - p.y;
            if (dx*dx + dy*dy <= (PIN_RAD+4)*(PIN_RAD+4)) {
                outId = cv.id;
                return i;
            }
        }
    }
    outId = -1;
    return -1;
}

bool Canvas::hitRailTap(ImVec2 wp, ImVec2 origin, ImVec2 size, bool& outVdd, float& outWorldX) const
{
    float vddY = origin.y + RAIL_BAND * 0.5f;
    float gndY = origin.y + size.y - RAIL_BAND * 0.5f;
    ImVec2 ws  = w2s(wp, origin);

    auto check = [&](float sy, bool vdd) -> bool {
        float dy = ws.y - sy;
        if (std::fabs(dy) > (RAIL_BAND * 0.5f + 8.f)) return false;
        float snappedX = std::round(wp.x / SNAP) * SNAP;
        float dx = std::fabs(wp.x - snappedX);
        if (dx > SNAP * 0.6f) return false;
        outVdd = vdd;
        outWorldX = snappedX;
        return true;
    };

    if (check(vddY, true))  return true;
    if (check(gndY, false)) return true;
    return false;
}

int Canvas::hitJunction(ImVec2 wp) const
{
    for (const auto& j : junctions) {
        float dx = wp.x - j.pos.x, dy = wp.y - j.pos.y;
        if (dx*dx + dy*dy <= (PIN_RAD+5)*(PIN_RAD+5))
            return j.id;
    }
    return -1;
}

int Canvas::hitWire(ImVec2 wp, ImVec2 origin, ImVec2 size, ImVec2& outWorld) const
{
    const float thresh = 8.f / zoom;
    for (const auto& wv : wires) {
        ImVec2 src = endpointPos(wv.src, origin, size);
        ImVec2 dst = endpointPos(wv.dst, origin, size);
        auto pts = routeWire(src, dst, wv.src, wv.dst);
        for (size_t i = 1; i < pts.size(); ++i) {
            ImVec2 a = pts[i-1], b = pts[i];
            float minX = std::min(a.x, b.x) - thresh;
            float maxX = std::max(a.x, b.x) + thresh;
            float minY = std::min(a.y, b.y) - thresh;
            float maxY = std::max(a.y, b.y) + thresh;
            if (wp.x < minX || wp.x > maxX || wp.y < minY || wp.y > maxY) continue;

            if (std::fabs(a.y - b.y) < 0.1f) {
                if (std::fabs(wp.y - a.y) < thresh) {
                    outWorld = snapToGrid({ wp.x, wp.y });
                    return wv.id;
                }
            } else if (std::fabs(a.x - b.x) < 0.1f) {
                if (std::fabs(wp.x - a.x) < thresh) {
                    outWorld = snapToGrid({ wp.x, wp.y });
                    return wv.id;
                }
            }
        }
    }
    return -1;
}

bool Canvas::tryHandleComponentClick(int compId, bool mouseDown, bool mouseUp)
{
    auto* cv = findComp(compId);
    if (!cv) return false;

    if (cv->typeName == "SW" && mouseUp) {
        auto* sw = static_cast<Switch*>(cv->comp.get());
        sw->toggle();
        sim->settle();
        return true;
    }
    if (cv->typeName == "BTN") {
        auto* btn = static_cast<Button*>(cv->comp.get());
        if (mouseDown) { btn->press();   sim->settle(); return true; }
        if (mouseUp)   { btn->release(); sim->settle(); return true; }
    }
    if (cv->typeName == "NUM_IN" && mouseUp) {
        auto* ni = static_cast<NumericInput*>(cv->comp.get());
        ni->setValue((ni->getValue() + 1) & 0xF);
        sim->settle();
        return true;
    }
    return false;
}

void Canvas::handleScrollOnComponent(int compId, float scroll)
{
    auto* cv = findComp(compId);
    if (!cv || cv->typeName != "NUM_IN") return;
    auto* ni = static_cast<NumericInput*>(cv->comp.get());
    int v = ni->getValue();
    v = scroll > 0 ? (v + 1) & 0xF : (v - 1) & 0xF;
    ni->setValue(v);
    sim->settle();
}

std::vector<ImVec2> Canvas::routeWire(ImVec2 src, ImVec2 dst, const Endpoint& srcEp, const Endpoint& dstEp) const
{
    if (srcEp.kind == EndpointKind::Rail) {
        return { src, {dst.x, src.y}, dst };
    }
    if (dstEp.kind == EndpointKind::Rail) {
        return { src, {src.x, dst.y}, dst };
    }

    if (src.x < dst.x - 5.f) {
        float mx = (src.x + dst.x) * 0.5f;
        return { src, {mx, src.y}, {mx, dst.y}, dst };
    } else {
        float bypass = 28.f;
        float my     = (src.y + dst.y) * 0.5f;
        return { src,
                 {src.x + bypass, src.y},
                 {src.x + bypass, my},
                 {dst.x - bypass, my},
                 {dst.x - bypass, dst.y},
                 dst };
    }
}

void Canvas::drawGrid(ImDrawList* dl, ImVec2 origin, ImVec2 size) const
{
    ImU32 col = IM_COL32(55, 55, 68, 180); // sleek modern gray dots
    ImVec2 wMin = s2w(origin, origin);
    ImVec2 wMax = s2w({origin.x+size.x, origin.y+size.y}, origin);

    float startX = std::floor(wMin.x / GRID) * GRID;
    float startY = std::floor(wMin.y / GRID) * GRID;

    float topY = origin.y + RAIL_BAND;
    float botY = origin.y + size.y - RAIL_BAND;

    for (float wx = startX; wx <= wMax.x; wx += GRID) {
        for (float wy = startY; wy <= wMax.y; wy += GRID) {
            ImVec2 p = w2s({wx, wy}, origin);
            if (p.y >= topY && p.y <= botY) {
                dl->AddCircleFilled(p, 1.2f * zoom, col);
            }
        }
    }
}

void Canvas::drawRails(ImDrawList* dl, ImVec2 origin, ImVec2 size) const
{
    float vddY = origin.y + RAIL_BAND * 0.5f;
    float gndY = origin.y + size.y - RAIL_BAND * 0.5f;

    dl->AddRectFilled(origin, {origin.x + size.x, origin.y + RAIL_BAND},
                      IM_COL32(20, 28, 20, 255));
    dl->AddRectFilled({origin.x, origin.y + size.y - RAIL_BAND},
                      {origin.x + size.x, origin.y + size.y},
                      IM_COL32(15, 20, 32, 255));

    State vddSt = sim->getVddNet()->getState();
    State gndSt = sim->getGndNet()->getState();
    ImU32 vddCol = stateColor(vddSt);
    ImU32 gndCol = stateColor(gndSt);

    dl->AddLine({origin.x, vddY}, {origin.x + size.x, vddY}, vddCol, 3.f);
    dl->AddLine({origin.x, gndY}, {origin.x + size.x, gndY}, gndCol, 3.f);

    dl->AddText({origin.x + 4.f, vddY - 14.f}, vddCol, "VDD +");
    dl->AddText({origin.x + 4.f, gndY + 2.f},  gndCol, "GND -");

    ImVec2 wMin = s2w(origin, origin);
    ImVec2 wMax = s2w({origin.x + size.x, origin.y + size.y}, origin);
    float startX = std::floor(wMin.x / SNAP) * SNAP;

    for (float wx = startX; wx <= wMax.x; wx += SNAP) {
        ImVec2 tapS = w2s({wx, 0.f}, origin);
        tapS.y = vddY;
        dl->AddCircleFilled(tapS, 4.f, vddCol);
        tapS.y = gndY;
        dl->AddCircleFilled(tapS, 4.f, gndCol);
    }
}

static const char* pinLabel(const std::string& type, bool isInput, int idx, int busWidth)
{
    if (isInput) {
        if (type == "NOT" || type == "BUF") {
            if (idx == 0) return "A";
            if (idx == 1) return "V";
            if (idx == 2) return "G";
        }
        if (type == "AND" || type == "NAND" || type == "OR" || type == "NOR" || type == "XOR" || type == "XNOR") {
            if (idx == 0) return "A";
            if (idx == 1) return "B";
            if (idx == 2) return "V";
            if (idx == 3) return "G";
        }
        if (type == "SW" || type == "BTN" || type == "CLK") {
            if (idx == 0) return "V";
            if (idx == 1) return "G";
        }
        if (type == "LED") {
            if (idx == 0) return "A";
        }
        if (type == "NUM_DISP") {
            static const char* nb[] = {"3","2","1","0"};
            if (idx < 4) return nb[idx];
        }
        if (type == "BUS_MERGE") {
            static char buf[8];
            std::snprintf(buf, sizeof(buf), "%d", idx);
            return buf;
        }
        if (type == "BUS_SPLIT") {
            static char buf[16];
            std::snprintf(buf, sizeof(buf), "[%d]", busWidth);
            return buf;
        }
    } else {
        if (type == "BUS_MERGE") {
            static char buf[16];
            std::snprintf(buf, sizeof(buf), "[%d]", busWidth);
            return buf;
        }
        if (type == "BUS_SPLIT") {
            static char buf[8];
            std::snprintf(buf, sizeof(buf), "%d", idx);
            return buf;
        }
    }
    return "";
}

void Canvas::drawComp(ImDrawList* dl, const ComponentView& cv, ImVec2 origin) const
{
    ImVec2 tl = w2s(cv.pos, origin);
    ImVec2 br = w2s({cv.pos.x + cv.size.x, cv.pos.y + cv.size.y}, origin);

    ImU32 bodyCol   = IM_COL32(23, 23, 31, 255); // #17171F modern deep cardbg
    ImU32 borderCol = cv.selected ? IM_COL32(99, 102, 241, 255) // Indigo
                                  : IM_COL32(42, 43, 54, 255);  // #2A2B36 sleek border
    float rounding = 6.f * zoom;
    dl->AddRectFilled(tl, br, bodyCol, rounding);
    dl->AddRect      (tl, br, borderCol, rounding, 0, cv.selected ? 2.f : 1.f);

    float fontSize = 13.f * zoom;

    if (cv.typeName == "NUM_DISP") {
        auto* nd = static_cast<NumericDisplay*>(cv.comp.get());
        char valBuf[32];
        if (nd->hasAmbiguity())
            std::snprintf(valBuf, sizeof(valBuf), "?");
        else
            std::snprintf(valBuf, sizeof(valBuf), "%d", nd->getValue());
        char hexBuf[16];
        std::snprintf(hexBuf, sizeof(hexBuf), "0x%X", nd->hasAmbiguity() ? 0 : nd->getValue());

        float bigSize = 22.f * zoom;
        ImVec2 ts = ImGui::CalcTextSize(valBuf);
        float sc = bigSize / ImGui::GetFontSize();
        ImVec2 centre = { (tl.x + br.x) * .5f, (tl.y + br.y) * .5f - 6.f * zoom };
        dl->AddText(ImGui::GetFont(), bigSize,
                    { centre.x - ts.x * sc * .5f, centre.y - ts.y * sc * .5f },
                    IM_COL32(255, 220, 80, 255), valBuf);
        if (fontSize >= 8.f) {
            ImVec2 hs = ImGui::CalcTextSize(hexBuf);
            float hsc = (fontSize * 0.7f) / ImGui::GetFontSize();
            dl->AddText(ImGui::GetFont(), fontSize * 0.7f,
                        { centre.x - hs.x * hsc * .5f, centre.y + ts.y * sc * .4f },
                        IM_COL32(180, 180, 180, 200), hexBuf);
        }
    } else if (fontSize >= 8.f) {
        ImVec2 centre = { (tl.x + br.x) * .5f, (tl.y + br.y) * .5f };
        ImVec2 ts     = ImGui::CalcTextSize(cv.typeName.c_str());
        float scale   = fontSize / ImGui::GetFontSize();
        dl->AddText(ImGui::GetFont(), fontSize,
                    { centre.x - ts.x * scale * .5f, centre.y - ts.y * scale * .5f },
                    IM_COL32(210, 215, 230, 255), cv.typeName.c_str());
    }

    if (cv.typeName == "LED") {
        auto* led = static_cast<LED*>(cv.comp.get());
        if (led->isLit()) {
            ImVec2 c = { (tl.x+br.x)*.5f, (tl.y+br.y)*.5f };
            float r  = (br.x - tl.x) * .35f;
            dl->AddCircleFilled(c, r * 1.6f, IM_COL32(76,175,80,40));
            dl->AddCircleFilled(c, r,        IM_COL32(76,175,80,200));
        }
    }

    int bw = componentBusWidth(cv);

    for (int i = 0; i < cv.comp->numReceivers(); ++i) {
        if (cv.typeName == "BUS_SPLIT" && i > 0) continue;
        ImVec2 tip, edge;
        if (isBusComponent(cv.typeName) && i == 0 && cv.typeName == "BUS_SPLIT") {
            tip  = w2s(busReceiverPos(cv), origin);
            edge = w2s({cv.pos.x, busReceiverPos(cv).y}, origin);
        } else {
            tip  = w2s(receiverPos(cv, i), origin);
            edge = w2s(receiverEdgePos(cv, i), origin);
        }
        State st  = cv.comp->getReceiver(i)->getState();
        ImU32 col = stateColor(st);
        float lw  = (isBusComponent(cv.typeName) && cv.typeName == "BUS_SPLIT" && i == 0)
                    ? 4.f * zoom : 2.f * zoom;
        dl->AddLine(edge, tip, col, lw);
        dl->AddCircleFilled(tip, PIN_RAD * zoom, col);
 
        const char* lbl = pinLabel(cv.typeName, true, i, bw);
        if (*lbl && fontSize >= 9.f) {
            ImVec2 ts = ImGui::CalcTextSize(lbl);
            float sc = fontSize / ImGui::GetFontSize() * 0.75f;
            ImVec2 textPos;
            if (hasVddGndInputs(cv.typeName) && i == cv.comp->numReceivers() - 2) {
                // VDD: top border, center horizontally, offset downwards inside
                textPos = { edge.x - ts.x * sc * 0.5f, edge.y + 2.f * zoom };
            } else if (hasVddGndInputs(cv.typeName) && i == cv.comp->numReceivers() - 1) {
                // GND: bottom border, center horizontally, offset upwards inside
                textPos = { edge.x - ts.x * sc * 0.5f, edge.y - ts.y * sc - 2.f * zoom };
            } else {
                // Regular left-side inputs
                textPos = { edge.x + 3.f * zoom, edge.y - ts.y * sc * 0.5f };
            }
            dl->AddText(ImGui::GetFont(), fontSize * .75f, textPos,
                        IM_COL32(180,180,180,200), lbl);
        }
    }

    for (int i = 0; i < cv.comp->numDrivers(); ++i) {
        if (cv.typeName == "BUS_MERGE" && i > 0) continue;
        ImVec2 tip, edge;
        if (isBusComponent(cv.typeName) && i == 0 && cv.typeName == "BUS_MERGE") {
            tip  = w2s(busDriverPos(cv), origin);
            edge = w2s({cv.pos.x + cv.size.x, busDriverPos(cv).y}, origin);
        } else {
            tip  = w2s(driverPos(cv, i), origin);
            edge = w2s({cv.pos.x + cv.size.x, driverPos(cv, i).y}, origin);
        }
        State st  = cv.comp->getDriver(i)->getState();
        ImU32 col = stateColor(st);
        float lw  = (isBusComponent(cv.typeName) && cv.typeName == "BUS_MERGE" && i == 0)
                    ? 4.f * zoom : 2.f * zoom;
        dl->AddLine(edge, tip, col, lw);
        dl->AddCircleFilled(tip, PIN_RAD * zoom, col);

        const char* lbl = pinLabel(cv.typeName, false, i, bw);
        if (*lbl && fontSize >= 9.f) {
            ImVec2 ts = ImGui::CalcTextSize(lbl);
            float sc = fontSize / ImGui::GetFontSize() * 0.75f;
            dl->AddText(ImGui::GetFont(), fontSize * .75f,
                        { edge.x - ts.x * sc - 3.f, edge.y - ts.y * sc * .5f },
                        IM_COL32(180,180,180,200), lbl);
        }
    }

    if (cv.typeName == "SW") {
        auto* sw   = static_cast<Switch*>(cv.comp.get());
        bool  on   = sw->isOn();
        float cx   = (tl.x + br.x) * .5f;
        float cy   = (tl.y + br.y) * .5f;
        float rr   = 8.f * zoom;
        float tx   = on ? cx + rr*0.5f : cx - rr*0.5f;
        dl->AddCircleFilled({cx, cy}, rr*1.4f, IM_COL32(50,50,60,255));
        dl->AddCircle      ({cx, cy}, rr*1.4f, IM_COL32(100,100,120,255));
        dl->AddCircleFilled({tx, cy}, rr,
            on ? IM_COL32(76,175,80,255) : IM_COL32(33,150,243,255));
    }

    if (cv.typeName == "NUM_IN" && fontSize >= 8.f) {
        auto* ni = static_cast<NumericInput*>(cv.comp.get());
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%d", ni->getValue());
        ImVec2 ts = ImGui::CalcTextSize(buf);
        float sc = fontSize / ImGui::GetFontSize();
        dl->AddText(ImGui::GetFont(), fontSize,
                    { br.x - ts.x * sc - 4.f, tl.y + 3.f },
                    IM_COL32(255, 200, 80, 220), buf);
    }

    if (cv.typeName == "JUNCTION") {
        ImVec2 c = { (tl.x + br.x) * .5f, (tl.y + br.y) * .5f };
        dl->AddCircleFilled(c, 6.f * zoom, IM_COL32(200, 200, 100, 255));
    }
}

void Canvas::drawJunctions(ImDrawList* dl, ImVec2 origin, ImVec2 /*size*/) const
{
    for (const auto& j : junctions) {
        ImVec2 s = w2s(j.pos, origin);
        State st = j.net ? j.net->getState() : State::FLOATING;
        ImU32 col = stateColor(st);
        dl->AddCircleFilled(s, 5.f * zoom, col);
        ImU32 borderCol = j.selected ? IM_COL32(99, 102, 241, 255)
                                     : IM_COL32(255, 255, 255, 180);
        dl->AddCircle(s, 5.f * zoom, borderCol, 0, j.selected ? 2.f : 1.5f);
    }
}

void Canvas::drawAllWires(ImDrawList* dl, ImVec2 origin, ImVec2 size) const
{
    for (const auto& wv : wires) {
        ImVec2 src = endpointPos(wv.src, origin, size);
        ImVec2 dst = endpointPos(wv.dst, origin, size);
        auto pts   = routeWire(src, dst, wv.src, wv.dst);

        State st = State::FLOATING;
        if (wv.busWidth > 1 && !wv.busNets.empty())
            st = wv.busNets[0]->getState();
        else if (wv.net)
            st = wv.net->getState();

        ImU32 col = stateColor(st);
        float lw  = wv.busWidth > 1 ? 4.f * zoom : 2.f * zoom;

        if (wv.selected) {
            for (size_t i = 1; i < pts.size(); ++i) {
                dl->AddLine(w2s(pts[i-1], origin), w2s(pts[i], origin),
                            IM_COL32(99, 102, 241, 130), lw + 4.f * zoom);
            }
        }

        for (size_t i = 1; i < pts.size(); ++i)
            dl->AddLine(w2s(pts[i-1], origin), w2s(pts[i], origin), col, lw);

        dl->AddCircleFilled(w2s(src, origin), 3.5f * zoom, col);

        if (wv.busWidth > 1 && pts.size() >= 2) {
            ImVec2 mid = w2s(pts[pts.size()/2], origin);
            char lbl[8];
            std::snprintf(lbl, sizeof(lbl), "%d", wv.busWidth);
            dl->AddText({mid.x + 4.f, mid.y - 8.f}, IM_COL32(220, 220, 220, 200), lbl);
        }
    }
}

void Canvas::drawWireInProgress(ImDrawList* dl, ImVec2 origin, ImVec2 size, ImVec2 mouseSS) const
{
    if (mode != Mode::DrawingWire) return;

    ImVec2 srcW = endpointPos(wireSrc, origin, size);
    ImVec2 dstW = s2w(mouseSS, origin);
    auto   pts  = routeWire(srcW, dstW, wireSrc, Endpoint{});

    ImU32 col = IM_COL32(200, 200, 100, 180);
    for (size_t i = 1; i < pts.size(); ++i)
        dl->AddLine(w2s(pts[i-1], origin), w2s(pts[i], origin), col, 2.f * zoom);
}

void Canvas::drawPlacementGhost(ImDrawList* dl, ImVec2 origin, ImVec2 mouseSS) const
{
    if (mode != Mode::Placing) return;
    ImVec2 wPos = snapToGrid(s2w(mouseSS, origin));
    wPos.x -= COMP_W / 2.f;
    wPos.y -= 30.f;

    ImVec2 size = getComponentSize(pendingType, pendingBusWidth);
    ImVec2 tl = w2s(wPos, origin);
    ImVec2 br = w2s({wPos.x + size.x, wPos.y + size.y}, origin);
    dl->AddRectFilled(tl, br, IM_COL32(99, 102, 241, 50), 6.f*zoom);
    dl->AddRect(tl, br, IM_COL32(129, 140, 248, 180), 6.f*zoom, 0, 1.5f);
}

void Canvas::render()
{
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 size   = ImGui::GetContentRegionAvail();
    if (size.x < 10 || size.y < 10) return;

    ImGui::InvisibleButton("##canvas", size,
        ImGuiButtonFlags_MouseButtonLeft  |
        ImGuiButtonFlags_MouseButtonRight |
        ImGuiButtonFlags_MouseButtonMiddle);

    bool hovered = ImGui::IsItemHovered();
    ImVec2 mousePos = ImGui::GetMousePos();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(origin, {origin.x+size.x, origin.y+size.y}, true);

    dl->AddRectFilled(origin, {origin.x+size.x, origin.y+size.y},
                      IM_COL32(18, 18, 26, 255));
    drawGrid(dl, origin, size);
    drawRails(dl, origin, size);

    if (hovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.f) ||
                    ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.f))) {
        ImVec2 delta = ImGui::GetMouseDragDelta(
            ImGui::IsMouseDragging(ImGuiMouseButton_Right) ?
                ImGuiMouseButton_Right : ImGuiMouseButton_Middle, 0.f);
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
        pan.x -= delta.x / zoom;
        pan.y -= delta.y / zoom;
    }

    if (hovered) {
        float scroll = ImGui::GetIO().MouseWheel;
        if (scroll != 0.f) {
            ImVec2 wBefore = s2w(mousePos, origin);
            int hit = hitComp(wBefore);
            if (hit >= 0)
                handleScrollOnComponent(hit, scroll);
            else {
                zoom = std::clamp(zoom * (scroll > 0 ? 1.12f : 0.89f), 0.15f, 5.f);
                ImVec2 wAfter = s2w(mousePos, origin);
                pan.x += wBefore.x - wAfter.x;
                pan.y += wBefore.y - wAfter.y;
            }
        }
    }

    // Right-click → context menu
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImVec2 wp = s2w(mousePos, origin);
        int jId = hitJunction(wp);
        if (jId >= 0) {
            rightClickedJunctionId = jId;
            rightClickedCompId = -1;
            rightClickedWireId = -1;
            auto* j = findJunction(jId);
            if (j && !j->selected) {
                bool shiftHeld = ImGui::GetIO().KeyShift;
                if (!shiftHeld) {
                    for (auto& c : comps) c.selected = false;
                    for (auto& jc : junctions) jc.selected = false;
                    for (auto& w : wires) w.selected = false;
                    selectedId = -1;
                }
                j->selected = true;
            }
            ImGui::OpenPopup("##canvas_context_menu");
        } else {
            int compId = hitComp(wp);
            if (compId >= 0) {
                rightClickedCompId = compId;
                rightClickedJunctionId = -1;
                rightClickedWireId = -1;
                auto* cv = findComp(compId);
                if (cv && !cv->selected) {
                    bool shiftHeld = ImGui::GetIO().KeyShift;
                    if (!shiftHeld) {
                        for (auto& c : comps) c.selected = false;
                        for (auto& jc : junctions) jc.selected = false;
                        for (auto& w : wires) w.selected = false;
                    }
                    cv->selected = true;
                    selectedId = compId;
                }
                ImGui::OpenPopup("##canvas_context_menu");
            } else {
                ImVec2 jPos;
                int wireId = hitWire(wp, origin, size, jPos);
                if (wireId >= 0) {
                    rightClickedWireId = wireId;
                    rightClickedWireJunctionPos = jPos;
                    rightClickedCompId = -1;
                    rightClickedJunctionId = -1;
                    for (auto& w : wires) {
                        if (w.id == wireId) {
                            if (!w.selected) {
                                bool shiftHeld = ImGui::GetIO().KeyShift;
                                if (!shiftHeld) {
                                    for (auto& c : comps) c.selected = false;
                                    for (auto& jc : junctions) jc.selected = false;
                                    for (auto& wr : wires) wr.selected = false;
                                    selectedId = -1;
                                }
                                w.selected = true;
                            }
                            break;
                        }
                    }
                    ImGui::OpenPopup("##canvas_context_menu");
                } else {
                    rightClickedCompId = -1;
                    rightClickedJunctionId = -1;
                    rightClickedWireId = -1;
                    ImGui::OpenPopup("##canvas_context_menu");
                }
            }
        }
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImVec2 wp = s2w(mousePos, origin);
        clickStartMouse = wp;

        if (mode == Mode::Placing) {
            placeAt(pendingType, wp, pendingBusWidth);
            if (!ImGui::GetIO().KeyShift) mode = Mode::Idle;

        } else if (mode == Mode::DrawingWire) {
            Endpoint dst;
            int compId, pinIdx;

            pinIdx = hitReceiverPin(wp, compId, true);
            if (pinIdx >= 0) {
                dst.kind = EndpointKind::Component;
                dst.compId = compId;
                dst.pinIdx = pinIdx;
                int bw = 1;
                auto* dstCv = findComp(compId);
                auto* srcCv = wireSrc.kind == EndpointKind::Component
                              ? findComp(wireSrc.compId) : nullptr;
                if (dstCv && dstCv->typeName == "BUS_SPLIT" && pinIdx == 0)
                    bw = dstCv->busWidth;
                else if (srcCv && srcCv->typeName == "BUS_MERGE")
                    bw = srcCv->busWidth;
                completeWire(wireSrc, dst, bw);
            } else {
                pinIdx = hitReceiverPin(wp, compId, false);
                if (pinIdx >= 0) {
                    dst.kind = EndpointKind::Component;
                    dst.compId = compId;
                    dst.pinIdx = pinIdx;
                    completeWire(wireSrc, dst, 1);
                } else {
                    bool railVdd; float railX;
                    if (hitRailTap(wp, origin, size, railVdd, railX)) {
                        dst.kind = EndpointKind::Rail;
                        dst.railIsVdd = railVdd;
                        dst.railX = railX;
                        completeWire(wireSrc, dst, 1);
                    } else {
                        int jId = hitJunction(wp);
                        if (jId >= 0) {
                            dst.kind = EndpointKind::Junction;
                            dst.junctionId = jId;
                            completeWire(wireSrc, dst, 1);
                        }
                    }
                }
            }
            mode = Mode::Idle;

        } else {
            int compId, pinIdx;

            pinIdx = hitDriverPin(wp, compId, true);
            if (pinIdx >= 0) {
                wireSrc.kind = EndpointKind::Component;
                wireSrc.compId = compId;
                wireSrc.pinIdx = pinIdx;
                wireSrc.isDriver = true;
                mode = Mode::DrawingWire;
            } else {
                pinIdx = hitDriverPin(wp, compId, false);
                if (pinIdx >= 0) {
                    wireSrc.kind = EndpointKind::Component;
                    wireSrc.compId = compId;
                    wireSrc.pinIdx = pinIdx;
                    wireSrc.isDriver = true;
                    mode = Mode::DrawingWire;
                } else {
                    bool railVdd; float railX;
                    if (hitRailTap(wp, origin, size, railVdd, railX)) {
                        wireSrc.kind = EndpointKind::Rail;
                        wireSrc.railIsVdd = railVdd;
                        wireSrc.railX = railX;
                        mode = Mode::DrawingWire;
                    } else {
                        int jId = hitJunction(wp);
                        if (jId >= 0) {
                            auto* j = findJunction(jId);
                            bool shiftHeld = ImGui::GetIO().KeyShift;
                            if (!j->selected) {
                                if (!shiftHeld) {
                                    for (auto& c : comps) c.selected = false;
                                    for (auto& jc : junctions) jc.selected = false;
                                }
                                j->selected = true;
                            }
                            mode = Mode::DraggingComp;
                            dragStartMouse = wp;
                            
                            dragStartComps.clear();
                            dragStartJunctions.clear();
                            for (const auto& c : comps) {
                                if (c.selected) dragStartComps.push_back({c.id, c.pos});
                            }
                            for (const auto& jc : junctions) {
                                if (jc.selected) dragStartJunctions.push_back({jc.id, jc.pos});
                            }
                        } else {
                            int hit = hitComp(wp);
                            if (hit >= 0) {
                                auto* cv = findComp(hit);
                                if (cv->typeName == "BTN") {
                                    tryHandleComponentClick(hit, true, false);
                                    pressedButtonId = hit;
                                    mode = Mode::PressingButton;
                                } else {
                                    bool shiftHeld = ImGui::GetIO().KeyShift;
                                    if (!cv->selected) {
                                        if (!shiftHeld) {
                                            for (auto& c : comps) c.selected = false;
                                            for (auto& jc : junctions) jc.selected = false;
                                            for (auto& w : wires) w.selected = false;
                                        }
                                        cv->selected = true;
                                    }
                                    selectedId = hit;
                                    mode = Mode::DraggingComp;
                                    dragStartMouse = wp;
                                    
                                    dragStartComps.clear();
                                    dragStartJunctions.clear();
                                    for (const auto& c : comps) {
                                        if (c.selected) dragStartComps.push_back({c.id, c.pos});
                                    }
                                    for (const auto& jc : junctions) {
                                        if (jc.selected) dragStartJunctions.push_back({jc.id, jc.pos});
                                    }
                                }
                            } else {
                                ImVec2 wirePos;
                                int wId = hitWire(wp, origin, size, wirePos);
                                if (wId >= 0) {
                                    bool shiftHeld = ImGui::GetIO().KeyShift;
                                    if (!shiftHeld) {
                                        for (auto& c : comps) c.selected = false;
                                        for (auto& jc : junctions) jc.selected = false;
                                        for (auto& w : wires) {
                                            if (w.id != wId) w.selected = false;
                                        }
                                        selectedId = -1;
                                    }
                                    for (auto& w : wires) {
                                        if (w.id == wId) {
                                            w.selected = shiftHeld ? !w.selected : true;
                                            break;
                                        }
                                    }
                                } else {
                                    // Clicked on empty space
                                    bool shiftHeld = ImGui::GetIO().KeyShift;
                                    if (!shiftHeld) {
                                        for (auto& c : comps) c.selected = false;
                                        for (auto& jc : junctions) jc.selected = false;
                                        for (auto& w : wires) w.selected = false;
                                        selectedId = -1;
                                    }
                                    mode = Mode::SelectingRegion;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (mode == Mode::SelectingRegion && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2 wp = s2w(mousePos, origin);
        float xmin = std::min(clickStartMouse.x, wp.x);
        float xmax = std::max(clickStartMouse.x, wp.x);
        float ymin = std::min(clickStartMouse.y, wp.y);
        float ymax = std::max(clickStartMouse.y, wp.y);

        bool shiftHeld = ImGui::GetIO().KeyShift;
        for (auto& cv : comps) {
            float cx = cv.pos.x + cv.size.x * 0.5f;
            float cy = cv.pos.y + cv.size.y * 0.5f;
            bool inside = (cx >= xmin && cx <= xmax && cy >= ymin && cy <= ymax);
            if (inside) {
                cv.selected = shiftHeld ? !cv.selected : true;
            } else {
                if (!shiftHeld) cv.selected = false;
            }
        }
        for (auto& j : junctions) {
            bool inside = (j.pos.x >= xmin && j.pos.x <= xmax && j.pos.y >= ymin && j.pos.y <= ymax);
            if (inside) {
                j.selected = shiftHeld ? !j.selected : true;
            } else {
                if (!shiftHeld) j.selected = false;
            }
        }
        mode = Mode::Idle;
    }

    if (mode == Mode::PressingButton && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (pressedButtonId >= 0)
            tryHandleComponentClick(pressedButtonId, false, true);
        pressedButtonId = -1;
        mode = Mode::Idle;
    }

    if (mode == Mode::DraggingComp && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2 wp = s2w(mousePos, origin);
        float dx = wp.x - clickStartMouse.x;
        float dy = wp.y - clickStartMouse.y;
        if (dx*dx + dy*dy < CLICK_THRESH * CLICK_THRESH) {
            if (selectedId >= 0) {
                tryHandleComponentClick(selectedId, false, true);
            }
        }
        mode = Mode::Idle;
    }

    if (mode == Mode::DraggingComp && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2 wp = s2w(mousePos, origin);
        ImVec2 delta = { wp.x - dragStartMouse.x, wp.y - dragStartMouse.y };
        for (const auto& p : dragStartComps) {
            if (auto* cv = findComp(p.first)) {
                cv->pos = snapToGrid({ p.second.x + delta.x, p.second.y + delta.y });
            }
        }
        for (const auto& p : dragStartJunctions) {
            if (auto* j = findJunction(p.first)) {
                j->pos = snapToGrid({ p.second.x + delta.x, p.second.y + delta.y });
            }
        }
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        mode = Mode::Idle;
        for (auto& cv : comps) cv.selected = false;
        for (auto& j : junctions) j.selected = false;
        for (auto& w : wires) w.selected = false;
        selectedId = -1;
        pressedButtonId = -1;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) deleteSelected();

    drawAllWires(dl, origin, size);
    drawJunctions(dl, origin, size);
    for (const auto& cv : comps) drawComp(dl, cv, origin);
    drawWireInProgress(dl, origin, size, mousePos);
    drawPlacementGhost(dl, origin, mousePos);

    if (mode == Mode::SelectingRegion) {
        ImVec2 wp = s2w(mousePos, origin);
        ImVec2 tl = w2s({std::min(clickStartMouse.x, wp.x), std::min(clickStartMouse.y, wp.y)}, origin);
        ImVec2 br = w2s({std::max(clickStartMouse.x, wp.x), std::max(clickStartMouse.y, wp.y)}, origin);
        dl->AddRectFilled(tl, br, IM_COL32(99, 102, 241, 35), 0.f);
        dl->AddRect(tl, br, IM_COL32(99, 102, 241, 180), 0.f, 0, 1.5f);
    }

    if (ImGui::BeginPopup("##canvas_context_menu")) {
        bool hasSel = hasSelection();
        if (rightClickedCompId >= 0) {
            auto* cv = findComp(rightClickedCompId);
            ImGui::Text("Component: %s", cv ? cv->typeName.c_str() : "");
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Component")) {
                if (cv) {
                    removeWiresOf(rightClickedCompId);
                    auto it = std::find_if(comps.begin(), comps.end(),
                                           [&](const ComponentView& c){ return c.id == rightClickedCompId; });
                    if (it != comps.end()) {
                        if (auto* clk = dynamic_cast<Clock*>(it->comp.get()))
                            sim->unregisterClock(clk);
                        else
                            sim->unregisterComponent(it->comp.get());
                        comps.erase(it);
                    }
                    if (selectedId == rightClickedCompId) selectedId = -1;
                    sim->settle();
                }
            }
        } else if (rightClickedJunctionId >= 0) {
            ImGui::Text("Junction");
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Junction")) {
                removeJunction(rightClickedJunctionId);
                std::vector<WireView> remainingWires;
                for (auto& wv : wires) {
                    bool touchesJunction = 
                        (wv.src.kind == EndpointKind::Junction && wv.src.junctionId == rightClickedJunctionId) ||
                        (wv.dst.kind == EndpointKind::Junction && wv.dst.junctionId == rightClickedJunctionId);
                    if (touchesJunction) {
                        if (wv.src.kind == EndpointKind::Component) {
                            if (auto* cv = findComp(wv.src.compId)) {
                                if (wv.busWidth <= 1) {
                                    if (auto* d = cv->comp->getDriver(wv.src.pinIdx)) sim->disconnectDriver(d);
                                } else {
                                    for (int i = 0; i < wv.busWidth; ++i) sim->disconnectDriver(cv->comp->getDriver(i));
                                }
                            }
                        }
                        if (wv.dst.kind == EndpointKind::Component) {
                            if (auto* cv = findComp(wv.dst.compId)) {
                                if (wv.busWidth <= 1) {
                                    sim->disconnectReceiver(cv->comp->getReceiver(wv.dst.pinIdx));
                                } else {
                                    for (int i = 0; i < wv.busWidth; ++i) sim->disconnectReceiver(cv->comp->getReceiver(i));
                                }
                            }
                        }
                    } else {
                        remainingWires.push_back(wv);
                    }
                }
                wires = std::move(remainingWires);
                cleanupDanglingJunctions();
                sim->settle();
            }
        } else if (rightClickedWireId >= 0) {
            ImGui::Text("Connection");
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Connection")) {
                removeWire(rightClickedWireId);
                sim->settle();
            }
            if (ImGui::MenuItem("Insert Junction")) {
                insertJunctionOnWire(rightClickedWireId, rightClickedWireJunctionPos);
                sim->settle();
            }
        } else {
            ImGui::Text("Canvas");
            ImGui::Separator();
            if (ImGui::MenuItem("Select All")) {
                for (auto& cv : comps) cv.selected = true;
                for (auto& j : junctions) j.selected = true;
                for (auto& w : wires) w.selected = true;
            }
            if (ImGui::MenuItem("Clear Selection", nullptr, false, hasSel)) {
                clearSelection();
            }
        }

        if (hasSel) {
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Selected")) {
                deleteSelected();
                sim->settle();
            }
        }
        ImGui::EndPopup();
    }

    if (mode == Mode::Placing || mode == Mode::DrawingWire || mode == Mode::SelectingRegion)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    dl->PopClipRect();
}
