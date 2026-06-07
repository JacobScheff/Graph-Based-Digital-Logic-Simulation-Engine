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

ImVec2 Canvas::receiverPos(const ComponentView& cv, int idx) const
{
    int n = cv.comp->numReceivers();
    if (n == 0) return cv.pos;
    float step = cv.size.y / float(n + 1);
    return { cv.pos.x - PIN_LEN,
             cv.pos.y + step * float(idx + 1) };
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
                if (cv->typeName == "BUS_MERGE")
                    return busDriverPos(*cv);
                if (cv->typeName == "BUS_SPLIT")
                    return busReceiverPos(*cv);
            }
            if (ep.pinIdx >= 0 && ep.pinIdx < cv->comp->numDrivers())
                return driverPos(*cv, ep.pinIdx);
            if (ep.pinIdx >= 0 && ep.pinIdx < cv->comp->numReceivers())
                return receiverPos(*cv, ep.pinIdx);
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

Canvas::ComponentView Canvas::makeView(const std::string& type, ImVec2 worldPos, int busWidth)
{
    ComponentView cv;
    cv.id       = nextId++;
    cv.typeName = type;
    cv.busWidth = busWidth;
    cv.comp     = makeComponent(type, busWidth);
    cv.comp->setBusWidth(busWidth);
    cv.pos      = worldPos;

    int maxPins = std::max(cv.comp->numReceivers(), cv.comp->numDrivers());
    float h = std::max(50.f, float(maxPins + 1) * PIN_SPACE);
    if (isBusComponent(type))
        h = std::max(h, float(busWidth + 1) * PIN_SPACE);
    cv.size = { COMP_W, h };

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
                                const Endpoint& src, const Endpoint& dst)
{
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

void Canvas::completeWire(const Endpoint& src, const Endpoint& dst, int busWidth)
{
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
        else if (src.kind == EndpointKind::Junction)
            if (auto* j = findJunction(src.junctionId)) srcNet = j->net;

        if (dst.kind == EndpointKind::Rail)
            dstNet = dst.railIsVdd ? sim->getVddNet() : sim->getGndNet();
        else if (dst.kind == EndpointKind::Junction)
            if (auto* j = findJunction(dst.junctionId)) dstNet = j->net;

        Net* wireNet = nullptr;

        if (drv && rcv) {
            if (rcv->isConnected()) return;
            wireNet = drv->isConnected() ? drv->getNet() : sim->connectDriver(drv);
            if (!wireNet || !sim->canConnect(drv, rcv, wireNet)) return;
            sim->connectReceiver(rcv, wireNet);
        } else if (drv && dstNet) {
            sim->connectDriver(drv, dstNet);
            wireNet = dstNet;
        } else if (srcNet && rcv) {
            if (rcv->isConnected()) return;
            sim->connectReceiver(rcv, srcNet);
            wireNet = srcNet;
        } else {
            return;
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
                if (wv.net->getDrivers().empty() && wv.net->getReceivers().empty())
                    sim->removeNet(wv.net);
            }
            for (Net* bn : wv.busNets) {
                if (bn && bn != sim->getVddNet() && bn != sim->getGndNet()
                    && bn->getDrivers().empty() && bn->getReceivers().empty())
                    sim->removeNet(bn);
            }
        } else {
            remaining.push_back(wv);
        }
    }
    wires = std::move(remaining);
}

void Canvas::removeJunction(int junctionId)
{
    junctions.erase(std::remove_if(junctions.begin(), junctions.end(),
        [&](const JunctionView& j){ return j.id == junctionId; }), junctions.end());
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
    if (selectedId < 0) return;
    removeWiresOf(selectedId);

    auto it = std::find_if(comps.begin(), comps.end(),
                           [&](const ComponentView& cv){ return cv.id == selectedId; });
    if (it != comps.end()) {
        if (auto* clk = dynamic_cast<Clock*>(it->comp.get()))
            sim->unregisterClock(clk);
        else
            sim->unregisterComponent(it->comp.get());
        comps.erase(it);
    }
    selectedId = -1;
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
        auto pts = routeWire(src, dst);
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

std::vector<ImVec2> Canvas::routeWire(ImVec2 src, ImVec2 dst) const
{
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
    ImU32 col = IM_COL32(50, 50, 60, 255);
    ImVec2 wMin = s2w(origin, origin);
    ImVec2 wMax = s2w({origin.x+size.x, origin.y+size.y}, origin);

    float startX = std::floor(wMin.x / GRID) * GRID;
    float startY = std::floor(wMin.y / GRID) * GRID;

    float topY = origin.y + RAIL_BAND;
    float botY = origin.y + size.y - RAIL_BAND;

    for (float wx = startX; wx <= wMax.x; wx += GRID) {
        ImVec2 a = w2s({wx, wMin.y}, origin);
        ImVec2 b = w2s({wx, wMax.y}, origin);
        a.y = std::max(a.y, topY);
        b.y = std::min(b.y, botY);
        if (a.y < b.y) dl->AddLine(a, b, col, 0.5f);
    }
    for (float wy = startY; wy <= wMax.y; wy += GRID) {
        if (wy < wMin.y + RAIL_BAND || wy > wMax.y - RAIL_BAND) continue;
        ImVec2 a = w2s({wMin.x, wy}, origin);
        ImVec2 b = w2s({wMax.x, wy}, origin);
        dl->AddLine(a, b, col, 0.5f);
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
    static const char* gateIn2[] = {"A","B"};
    if (isInput  && (type=="AND"||type=="NAND"||type=="OR"||
                     type=="NOR"||type=="XOR"||type=="XNOR")) return gateIn2[idx];
    if (isInput  && (type=="NOT"||type=="BUF"||type=="JUNCTION"||type=="LED"))
        return "A";
    if (type=="NUM_DISP"&& isInput) {
        static const char* nb[] = {"3","2","1","0"};
        if (idx < 4) return nb[idx];
    }
    if (type == "BUS_MERGE" && isInput) {
        static char buf[8];
        std::snprintf(buf, sizeof(buf), "%d", idx);
        return buf;
    }
    if (type == "BUS_SPLIT" && !isInput) {
        static char buf[8];
        std::snprintf(buf, sizeof(buf), "%d", idx);
        return buf;
    }
    if ((type == "BUS_MERGE" && !isInput) || (type == "BUS_SPLIT" && isInput)) {
        static char buf[16];
        std::snprintf(buf, sizeof(buf), "[%d]", busWidth);
        return buf;
    }
    return "";
}

void Canvas::drawComp(ImDrawList* dl, const ComponentView& cv, ImVec2 origin) const
{
    ImVec2 tl = w2s(cv.pos, origin);
    ImVec2 br = w2s({cv.pos.x + cv.size.x, cv.pos.y + cv.size.y}, origin);

    ImU32 bodyCol   = IM_COL32( 30,  32,  48, 255);
    ImU32 borderCol = cv.selected ? IM_COL32(130, 130, 255, 255)
                                  : IM_COL32( 80,  82, 110, 255);
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
        ImVec2 tip, edge;
        if (isBusComponent(cv.typeName) && i == 0 && cv.typeName == "BUS_SPLIT") {
            tip  = w2s(busReceiverPos(cv), origin);
            edge = w2s({cv.pos.x, busReceiverPos(cv).y}, origin);
        } else {
            tip  = w2s(receiverPos(cv, i), origin);
            edge = w2s({cv.pos.x, receiverPos(cv, i).y}, origin);
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
            dl->AddText(ImGui::GetFont(), fontSize * .75f,
                        { edge.x + 3.f, edge.y - ts.y * sc * .5f },
                        IM_COL32(180,180,180,200), lbl);
        }
    }

    for (int i = 0; i < cv.comp->numDrivers(); ++i) {
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
        dl->AddCircle(s, 5.f * zoom, IM_COL32(255, 255, 255, 180), 0, 1.5f);
    }
}

void Canvas::drawAllWires(ImDrawList* dl, ImVec2 origin, ImVec2 size) const
{
    for (const auto& wv : wires) {
        ImVec2 src = endpointPos(wv.src, origin, size);
        ImVec2 dst = endpointPos(wv.dst, origin, size);
        auto pts   = routeWire(src, dst);

        State st = State::FLOATING;
        if (wv.busWidth > 1 && !wv.busNets.empty())
            st = wv.busNets[0]->getState();
        else if (wv.net)
            st = wv.net->getState();

        ImU32 col = stateColor(st);
        float lw  = wv.busWidth > 1 ? 4.f * zoom : 2.f * zoom;

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
    auto   pts  = routeWire(srcW, dstW);

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

    auto tmp = const_cast<Canvas*>(this)->makeView(pendingType, wPos, pendingBusWidth);
    ImVec2 tl = w2s(tmp.pos, origin);
    ImVec2 br = w2s({tmp.pos.x + tmp.size.x, tmp.pos.y + tmp.size.y}, origin);
    dl->AddRectFilled(tl, br, IM_COL32(100,100,200,50), 6.f*zoom);
    dl->AddRect(tl, br, IM_COL32(150,150,255,180), 6.f*zoom);

    if (auto* clk = dynamic_cast<Clock*>(tmp.comp.get()))
        const_cast<Canvas*>(this)->sim->unregisterClock(clk);
    else
        const_cast<Canvas*>(this)->sim->unregisterComponent(tmp.comp.get());
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

    // Right-click on wire → insert junction
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImVec2 wp = s2w(mousePos, origin);
        ImVec2 jPos;
        int wireId = hitWire(wp, origin, size, jPos);
        if (wireId >= 0)
            insertJunctionOnWire(wireId, jPos);
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
                mode = Mode::DrawingWire;
            } else {
                pinIdx = hitDriverPin(wp, compId, false);
                if (pinIdx >= 0) {
                    wireSrc.kind = EndpointKind::Component;
                    wireSrc.compId = compId;
                    wireSrc.pinIdx = pinIdx;
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
                            wireSrc.kind = EndpointKind::Junction;
                            wireSrc.junctionId = jId;
                            mode = Mode::DrawingWire;
                        } else {
                            int hit = hitComp(wp);
                            if (hit >= 0) {
                                auto* cv = findComp(hit);
                                if (cv->typeName == "BTN") {
                                    tryHandleComponentClick(hit, true, false);
                                    pressedButtonId = hit;
                                    mode = Mode::PressingButton;
                                } else {
                                    if (selectedId >= 0)
                                        if (auto* prev = findComp(selectedId)) prev->selected = false;
                                    selectedId = hit;
                                    cv->selected = true;
                                    mode = Mode::DraggingComp;
                                    dragStartMouse = wp;
                                    dragStartPos   = cv->pos;
                                }
                            } else {
                                if (selectedId >= 0)
                                    if (auto* prev = findComp(selectedId)) prev->selected = false;
                                selectedId = -1;
                            }
                        }
                    }
                }
            }
        }
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
        if (dx*dx + dy*dy < CLICK_THRESH * CLICK_THRESH)
            tryHandleComponentClick(selectedId, false, true);
        mode = Mode::Idle;
    }

    if (mode == Mode::DraggingComp && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2 wp = s2w(mousePos, origin);
        if (auto* cv = findComp(selectedId)) {
            ImVec2 newPos = { dragStartPos.x + (wp.x - dragStartMouse.x),
                              dragStartPos.y + (wp.y - dragStartMouse.y) };
            cv->pos = snapToGrid(newPos);
        }
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        mode = Mode::Idle;
        if (selectedId >= 0)
            if (auto* cv = findComp(selectedId)) cv->selected = false;
        selectedId = -1;
        pressedButtonId = -1;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) deleteSelected();

    drawAllWires(dl, origin, size);
    drawJunctions(dl, origin, size);
    for (const auto& cv : comps) drawComp(dl, cv, origin);
    drawWireInProgress(dl, origin, size, mousePos);
    drawPlacementGhost(dl, origin, mousePos);

    if (mode == Mode::Placing || mode == Mode::DrawingWire)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    dl->PopClipRect();
}
