#include "Canvas.hpp"
#include "Gates.hpp"
#include "IO.hpp"
#include "Net.hpp"
#include "Pin.hpp"
#include "PowerRails.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr float kRgbChannelT[3] = {0.25f, 0.5f, 0.75f};
constexpr const char* kRgbChannelNames[3] = {"R", "G", "B"};
constexpr ImU32 kRgbChannelLabelColors[3] = {
    IM_COL32(235, 90, 90, 230),
    IM_COL32(90, 210, 100, 230),
    IM_COL32(90, 150, 235, 230),
};
} // namespace

// ─── Utility ──────────────────────────────────────────────────────────────────

ImU32 Canvas::stateColor(State s) const
{
    if (sim) return stateColorRail(s, *sim);
    switch (s) {
        case State::LOW:       return IM_COL32( 64, 140, 230, 255);
        case State::HIGH:      return IM_COL32( 76, 185,  80, 255);
        case State::FLOATING:  return IM_COL32(115, 115, 125, 255);
        case State::UNDEFINED: return IM_COL32(235,  75,  65, 255);
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
    for (auto& cv : comps) {
        if (cv.comp) {
            sim->unregisterComponent(cv.comp.get());
        }
    }
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
    return vdd ? origin.y + RAIL_BAND * 0.5f : origin.y;
}

ImVec2 Canvas::railTapWorldX(float worldX) const
{
    return { std::round(worldX / SNAP) * SNAP, 0.f };
}

// ─── Pin Layout ───────────────────────────────────────────────────────────────

ImVec2 Canvas::pinWorldPos(const ComponentView& cv, const PinLayout& pl, bool isEdge) const
{
    float offset = isEdge ? 0.f : PIN_LEN;
    switch (pl.side) {
        case 0: // Left
            return { cv.pos.x - (isEdge ? 0.f : offset),
                     cv.pos.y + pl.t * cv.size.y };
        case 1: // Top
            return { cv.pos.x + pl.t * cv.size.x,
                     cv.pos.y - (isEdge ? 0.f : offset) };
        case 2: // Right
            return { cv.pos.x + cv.size.x + (isEdge ? 0.f : offset),
                     cv.pos.y + pl.t * cv.size.y };
        case 3: // Bottom
            return { cv.pos.x + pl.t * cv.size.x,
                     cv.pos.y + cv.size.y + (isEdge ? 0.f : offset) };
    }
    return cv.pos;
}

void Canvas::initPinLayouts(ComponentView& cv)
{
    auto it = customDefs.find(cv.typeName);
    if (it != customDefs.end()) {
        // Custom components: use definition's side/order
        const auto& d = it->second;
        cv.receiverLayout.clear();
        for (const auto& p : d.inPorts) {
            if (p.t >= 0.f) {
                for (int b = 0; b < p.busWidth; ++b) {
                    PinLayout pl;
                    pl.side = p.side;
                    pl.t = p.t;
                    cv.receiverLayout.push_back(pl);
                }
            } else {
                int totalOnSide = 0;
                int startOnSide = 0;
                for (const auto& ip : d.inPorts) {
                    if (ip.side == p.side) {
                        if (&ip < &p) startOnSide += ip.busWidth;
                        totalOnSide += ip.busWidth;
                    }
                }
                for (int b = 0; b < p.busWidth; ++b) {
                    PinLayout pl;
                    pl.side = p.side;
                    pl.t = float(totalOnSide - (startOnSide + b)) / float(totalOnSide + 1);
                    cv.receiverLayout.push_back(pl);
                }
            }
        }
        cv.driverLayout.clear();
        for (const auto& p : d.outPorts) {
            if (p.t >= 0.f) {
                for (int b = 0; b < p.busWidth; ++b) {
                    PinLayout pl;
                    pl.side = p.side;
                    pl.t = p.t;
                    cv.driverLayout.push_back(pl);
                }
            } else {
                int totalOnSide = 0;
                int startOnSide = 0;
                for (const auto& op : d.outPorts) {
                    if (op.side == p.side) {
                        if (&op < &p) startOnSide += op.busWidth;
                        totalOnSide += op.busWidth;
                    }
                }
                for (int b = 0; b < p.busWidth; ++b) {
                    PinLayout pl;
                    pl.side = p.side;
                    pl.t = float(totalOnSide - (startOnSide + b)) / float(totalOnSide + 1);
                    cv.driverLayout.push_back(pl);
                }
            }
        }
        return;
    }

    if (cv.typeName == "RGB_DISP") {
        int cw = cv.busWidth;
        int nRcv = cw * 3;
        cv.receiverLayout.resize(nRcv);
        for (int c = 0; c < 3; ++c) {
            for (int b = 0; b < cw; ++b) {
                PinLayout pl;
                pl.side = 0;
                pl.t = kRgbChannelT[c];
                cv.receiverLayout[c * cw + b] = pl;
            }
        }
        cv.driverLayout.clear();
        return;
    }

    if (cv.typeName == "NUM_IN") {
        cv.receiverLayout.clear();
        cv.driverLayout = {{2, 0.5f}};
        return;
    }
    if (cv.typeName == "NUM_DISP") {
        cv.receiverLayout = {{0, 0.5f}};
        cv.driverLayout.clear();
        return;
    }

    int nRcv = cv.comp ? cv.comp->numReceivers() : 0;
    int nDrv = cv.comp ? cv.comp->numDrivers()   : 0;

    // Receivers
    cv.receiverLayout.resize(nRcv);
    for (int i = 0; i < nRcv; ++i) {
        cv.receiverLayout[i].side = 0; // Left
        cv.receiverLayout[i].t = float(nRcv - i) / float(nRcv + 1);
    }

    // Drivers
    cv.driverLayout.resize(nDrv);
    for (int i = 0; i < nDrv; ++i) {
        cv.driverLayout[i].side = 2; // Right
        cv.driverLayout[i].t = float(nDrv - i) / float(nDrv + 1);
    }
}

Canvas::PinLayout Canvas::projectOntoPerimeter(const ComponentView& cv, ImVec2 wp) const
{
    float cx = cv.pos.x + cv.size.x * 0.5f;
    float cy = cv.pos.y + cv.size.y * 0.5f;
    float dx = wp.x - cx;
    float dy = wp.y - cy;
    float hw = cv.size.x * 0.5f;
    float hh = cv.size.y * 0.5f;
    float sx = (hw > 0.f) ? dx / hw : 0.f;
    float sy = (hh > 0.f) ? dy / hh : 0.f;

    PinLayout pl;
    float margin = 0.08f;
    auto clampT = [margin](float v) { return std::clamp(v, margin, 1.f - margin); };

    if (std::fabs(sx) > std::fabs(sy)) {
        if (sx < 0) {
            pl.side = 0;
            pl.t = clampT((wp.y - cv.pos.y) / cv.size.y);
        } else {
            pl.side = 2;
            pl.t = clampT((wp.y - cv.pos.y) / cv.size.y);
        }
    } else {
        if (sy < 0) {
            pl.side = 1;
            pl.t = clampT((wp.x - cv.pos.x) / cv.size.x);
        } else {
            pl.side = 3;
            pl.t = clampT((wp.x - cv.pos.x) / cv.size.x);
        }
    }
    return pl;
}

// ─── Pin Position Accessors ───────────────────────────────────────────────────

ImVec2 Canvas::getCustomPinPos(const ComponentView& cv, int side, int totalOnSide, int idxOnSide, bool isEdge) const
{
    float step, offset;
    switch (side) {
        case 0: // Left
            step = cv.size.y / float(totalOnSide + 1);
            offset = isEdge ? 0 : -PIN_LEN;
            return { cv.pos.x + offset, cv.pos.y + step * float(idxOnSide + 1) };
        case 1: // Top
            step = cv.size.x / float(totalOnSide + 1);
            offset = isEdge ? 0 : -PIN_LEN;
            return { cv.pos.x + step * float(idxOnSide + 1), cv.pos.y + offset };
        case 2: // Right
            step = cv.size.y / float(totalOnSide + 1);
            offset = isEdge ? 0 : PIN_LEN;
            return { cv.pos.x + cv.size.x + offset, cv.pos.y + step * float(idxOnSide + 1) };
        case 3: // Bottom
            step = cv.size.x / float(totalOnSide + 1);
            offset = isEdge ? 0 : PIN_LEN;
            return { cv.pos.x + step * float(idxOnSide + 1), cv.pos.y + cv.size.y + offset };
    }
    return cv.pos;
}


ImVec2 Canvas::driverPos(const ComponentView& cv, int idx) const
{
    // Use PinLayout if available
    if (idx >= 0 && idx < (int)cv.driverLayout.size()) {
        return pinWorldPos(cv, cv.driverLayout[idx], false);
    }
    // Fallback: custom component or legacy
    auto it = customDefs.find(cv.typeName);
    if (it != customDefs.end()) {
        const auto& d = it->second;
        int currentIdx = 0;
        for (const auto& p : d.outPorts) {
            if (idx >= currentIdx && idx < currentIdx + p.busWidth) {
                int totalOnSide = 0;
                int startOnSide = 0;
                for (const auto& op : d.outPorts) {
                    if (op.side == p.side) {
                        if (&op < &p) startOnSide += op.busWidth;
                        totalOnSide += op.busWidth;
                    }
                }
                return getCustomPinPos(cv, p.side, totalOnSide, startOnSide + (idx - currentIdx), false);
            }
            currentIdx += p.busWidth;
        }
    }
    int n = cv.comp->numDrivers();
    if (n == 0) return cv.pos;
    float step = cv.size.y / float(n + 1);
    return { cv.pos.x + cv.size.x + PIN_LEN,
             cv.pos.y + step * float(idx + 1) };
}

ImVec2 Canvas::driverEdgePos(const ComponentView& cv, int idx) const
{
    if (idx >= 0 && idx < (int)cv.driverLayout.size()) {
        return pinWorldPos(cv, cv.driverLayout[idx], true);
    }
    auto it = customDefs.find(cv.typeName);
    if (it != customDefs.end()) {
        const auto& d = it->second;
        int currentIdx = 0;
        for (const auto& p : d.outPorts) {
            if (idx >= currentIdx && idx < currentIdx + p.busWidth) {
                int totalOnSide = 0;
                int startOnSide = 0;
                for (const auto& op : d.outPorts) {
                    if (op.side == p.side) {
                        if (&op < &p) startOnSide += op.busWidth;
                        totalOnSide += op.busWidth;
                    }
                }
                return getCustomPinPos(cv, p.side, totalOnSide, startOnSide + (idx - currentIdx), true);
            }
            currentIdx += p.busWidth;
        }
    }
    int n = cv.comp->numDrivers();
    if (n == 0) return cv.pos;
    float step = cv.size.y / float(n + 1);
    return { cv.pos.x + cv.size.x, cv.pos.y + step * float(idx + 1) };
}

ImVec2 Canvas::receiverPos(const ComponentView& cv, int idx) const
{
    if (idx >= 0 && idx < (int)cv.receiverLayout.size()) {
        return pinWorldPos(cv, cv.receiverLayout[idx], false);
    }
    auto it = customDefs.find(cv.typeName);
    if (it != customDefs.end()) {
        const auto& d = it->second;
        int currentIdx = 0;
        for (const auto& p : d.inPorts) {
            if (idx >= currentIdx && idx < currentIdx + p.busWidth) {
                int totalOnSide = 0;
                int startOnSide = 0;
                for (const auto& ip : d.inPorts) {
                    if (ip.side == p.side) {
                        if (&ip < &p) startOnSide += ip.busWidth;
                        totalOnSide += ip.busWidth;
                    }
                }
                return getCustomPinPos(cv, p.side, totalOnSide, startOnSide + (idx - currentIdx), false);
            }
            currentIdx += p.busWidth;
        }
    }
    int n = cv.comp->numReceivers();
    if (n == 0) return cv.pos;
    float step = cv.size.y / float(n + 1);
    return { cv.pos.x - PIN_LEN, cv.pos.y + step * float(idx + 1) };
}

ImVec2 Canvas::receiverEdgePos(const ComponentView& cv, int idx) const
{
    if (idx >= 0 && idx < (int)cv.receiverLayout.size()) {
        return pinWorldPos(cv, cv.receiverLayout[idx], true);
    }
    auto it = customDefs.find(cv.typeName);
    if (it != customDefs.end()) {
        const auto& d = it->second;
        int currentIdx = 0;
        for (const auto& p : d.inPorts) {
            if (idx >= currentIdx && idx < currentIdx + p.busWidth) {
                int totalOnSide = 0;
                int startOnSide = 0;
                for (const auto& ip : d.inPorts) {
                    if (ip.side == p.side) {
                        if (&ip < &p) startOnSide += ip.busWidth;
                        totalOnSide += ip.busWidth;
                    }
                }
                return getCustomPinPos(cv, p.side, totalOnSide, startOnSide + (idx - currentIdx), true);
            }
            currentIdx += p.busWidth;
        }
    }
    int n = cv.comp->numReceivers();
    if (n == 0) return cv.pos;
    return { cv.pos.x, receiverPos(cv, idx).y };
}

ImVec2 Canvas::busDriverPos(const ComponentView& cv) const
{
    if (!cv.driverLayout.empty()) {
        return driverPos(cv, 0);
    }
    return { cv.pos.x + cv.size.x + PIN_LEN, cv.pos.y + cv.size.y * 0.5f };
}

ImVec2 Canvas::busReceiverPos(const ComponentView& cv) const
{
    if (!cv.receiverLayout.empty()) {
        return receiverPos(cv, 0);
    }
    return { cv.pos.x - PIN_LEN, cv.pos.y + cv.size.y * 0.5f };
}

ImVec2 Canvas::endpointPos(const Endpoint& ep, ImVec2 origin, ImVec2 canvasSize) const
{
    switch (ep.kind) {
        case EndpointKind::Component: {
            const auto* cv = findComp(ep.compId);
            if (!cv) return {0, 0};
            if (ep.kind == EndpointKind::Component && ep.pinIdx >= 0) {
                if (hasConsolidatedBusDriver(*cv) && ep.isDriver &&
                    ep.pinIdx >= 0 && ep.pinIdx < cv->busWidth)
                    return busDriverPos(*cv);
                if (hasConsolidatedBusReceiver(*cv) && !ep.isDriver &&
                    ep.pinIdx >= 0 && ep.pinIdx < cv->busWidth)
                    return busReceiverPos(*cv);
            }
            if (isBusComponent(cv->typeName) && ep.pinIdx == 0) {
                if ((cv->typeName == "BUS_MERGE" || cv->typeName == "REG" || cv->typeName == "PORT_IN") && ep.isDriver)
                    return busDriverPos(*cv);
                if ((cv->typeName == "BUS_SPLIT" || cv->typeName == "REG" || cv->typeName == "PORT_OUT") && !ep.isDriver)
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

// ─── Component Factory ────────────────────────────────────────────────────────

bool Canvas::isBusComponent(const std::string& type) const
{
    return type == "BUS_MERGE" || type == "BUS_SPLIT" || type == "REG"
        || type == "PORT_IN" || type == "PORT_OUT"
        || type == "NUM_IN" || type == "NUM_DISP";
}

bool Canvas::hasConsolidatedBusDriver(const ComponentView& cv) const
{
    return cv.typeName == "BUS_MERGE" || cv.typeName == "REG"
        || cv.typeName == "PORT_IN" || cv.typeName == "NUM_IN";
}

bool Canvas::hasConsolidatedBusReceiver(const ComponentView& cv) const
{
    return cv.typeName == "BUS_SPLIT" || cv.typeName == "REG"
        || cv.typeName == "PORT_OUT" || cv.typeName == "NUM_DISP";
}

int Canvas::componentBusWidth(const ComponentView& cv) const
{
    if (isBusComponent(cv.typeName) || cv.typeName == "NUM_IN" || cv.typeName == "NUM_DISP")
        return cv.busWidth;
    if (cv.typeName == "RGB_DISP")
        return cv.busWidth;
    return 1;
}

bool Canvas::isCustomPortStart(const std::string& typeName, bool isInput, int pinIdx, int& outBusWidth) const
{
    auto it = customDefs.find(typeName);
    if (it != customDefs.end()) {
        const auto& ports = isInput ? it->second.inPorts : it->second.outPorts;
        int currentIdx = 0;
        for (const auto& p : ports) {
            if (pinIdx == currentIdx) {
                outBusWidth = p.busWidth;
                return true;
            }
            if (pinIdx > currentIdx && pinIdx < currentIdx + p.busWidth) {
                outBusWidth = -1; // Indicates it is strictly inside a bus port
                return false;
            }
            currentIdx += p.busWidth;
        }
    }
    outBusWidth = 1;
    return true; // Not a custom component or not inside a bus port
}

bool Canvas::isPortGroupStart(const ComponentView& cv, bool isInput, int pinIdx, int& outBusWidth) const
{
    if (cv.typeName == "NUM_IN" && !isInput) {
        if (pinIdx == 0) { outBusWidth = cv.busWidth; return true; }
        if (pinIdx > 0 && pinIdx < cv.busWidth) { outBusWidth = -1; return false; }
        return false;
    }
    if (cv.typeName == "NUM_DISP" && isInput) {
        if (pinIdx == 0) { outBusWidth = cv.busWidth; return true; }
        if (pinIdx > 0 && pinIdx < cv.busWidth) { outBusWidth = -1; return false; }
        return false;
    }
    if (cv.typeName == "RGB_DISP" && isInput) {
        int cw = cv.busWidth;
        if (pinIdx == 0 || pinIdx == cw || pinIdx == 2 * cw) {
            outBusWidth = cw;
            return true;
        }
        if (pinIdx < 3 * cw && pinIdx % cw != 0) {
            outBusWidth = -1;
            return false;
        }
        return false;
    }
    return isCustomPortStart(cv.typeName, isInput, pinIdx, outBusWidth);
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
    if (type == "NUM_IN")   return std::make_unique<NumericInput>(busWidth);
    if (type == "NUM_DISP") return std::make_unique<NumericDisplay>(busWidth);
    if (type == "RGB_DISP") return std::make_unique<RGBDisplay>(busWidth);
    if (type == "JUNCTION") return std::make_unique<Junction>();
    if (type == "BUS_MERGE") return std::make_unique<BusMerge>(busWidth);
    if (type == "BUS_SPLIT") return std::make_unique<BusSplit>(busWidth);
    if (type == "REG")       return std::make_unique<Register>(busWidth);
    if (type == "PORT_IN")   return std::make_unique<PortIn>(busWidth);
    if (type == "PORT_OUT")  return std::make_unique<PortOut>(busWidth);

    auto it = customDefs.find(type);
    if (it != customDefs.end()) {
        auto comp = std::make_unique<CustomComponent>(it->second);
        if (sim) comp->registerInternals(sim, customDefs);
        return comp;
    }
    
    return nullptr;
}

ImVec2 Canvas::getComponentSize(const std::string& type, int busWidth) const
{
    int numReceivers = 0;
    int numDrivers = 0;
    
    if (type == "NOT" || type == "BUF") {
        numReceivers = 1;
        numDrivers = 1;
    } else if (type == "AND" || type == "NAND" || type == "OR" || type == "NOR" || type == "XOR" || type == "XNOR") {
        numReceivers = 2;
        numDrivers = 1;
    } else if (type == "SW" || type == "BTN") {
        numReceivers = 0;
        numDrivers = 1;
    } else if (type == "CLK") {
        numReceivers = 0;
        numDrivers = 1;
    } else if (type == "LED") {
        numReceivers = 1;
        numDrivers = 0;
    } else if (type == "NUM_IN") {
        numReceivers = 0;
        numDrivers = busWidth;
    } else if (type == "NUM_DISP") {
        numReceivers = busWidth;
        numDrivers = 0;
    } else if (type == "RGB_DISP") {
        numReceivers = busWidth * 3;
        numDrivers = 0;
    } else if (type == "JUNCTION") {
        numReceivers = 1;
        numDrivers = 1;
    } else if (type == "BUS_MERGE" || type == "BUS_SPLIT") {
        numReceivers = busWidth;
        numDrivers = busWidth;
    } else if (type == "REG") {
        numReceivers = busWidth + 1;
        numDrivers = busWidth;
    } else if (type == "PORT_IN") {
        numReceivers = 0;
        numDrivers = busWidth;
    } else if (type == "PORT_OUT") {
        numReceivers = busWidth;
        numDrivers = 0;
    } else {
        auto it = customDefs.find(type);
        if (it != customDefs.end()) {
            return { it->second.width, it->second.height };
        }
    }
    
    int maxPins = std::max(numReceivers, numDrivers);
    float h = std::max(50.f, float(maxPins + 1) * PIN_SPACE);
    if (type == "NUM_IN" || type == "NUM_DISP") {
        return { COMP_W, 50.f };
    }
    if (type == "RGB_DISP") {
        h = std::max(90.f, 4.f * PIN_SPACE);
        return { COMP_W + 10.f, h };
    }
    if (type == "BUS_MERGE" || type == "BUS_SPLIT" || type == "REG") {
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
    if (!cv.comp) return cv; // Return invalid component rather than segfault
    cv.comp->setBusWidth(busWidth);
    cv.pos      = worldPos;
    cv.size     = getComponentSize(type, busWidth);

    // Initialize pin layouts with defaults
    initPinLayouts(cv);

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

    auto cv = makeView(type, worldPos, busWidth);
    if (!cv.comp) return;
    comps.push_back(std::move(cv));
    selectedId = comps.back().id;
}

void Canvas::beginPlacement(const std::string& typeName, int busWidth)
{
    mode            = Mode::Placing;
    pendingType     = typeName;
    pendingBusWidth = busWidth;
    selectedId      = -1;
}

// ─── Wire Completion ──────────────────────────────────────────────────────────

int Canvas::getAvailablePortWidth(const ComponentView* cv, int pinIdx, bool isInput) const
{
    if (!cv) return 1;

    auto it = customDefs.find(cv->typeName);
    if (it != customDefs.end()) {
        int customBw = 1;
        if (isPortGroupStart(*cv, isInput, pinIdx, customBw)) {
            return customBw;
        }
        return 1;
    }

    if (cv->typeName == "RGB_DISP") {
        if (isInput) {
            int cw = cv->busWidth;
            if (pinIdx == 0 || pinIdx == cw || pinIdx == 2 * cw)
                return cw;
        }
        return 1;
    }

    if (cv->typeName == "BUS_SPLIT") {
        if (isInput && pinIdx == 0) return cv->busWidth;
        return 1;
    }
    if (cv->typeName == "BUS_MERGE") {
        if (!isInput && pinIdx == 0) return cv->busWidth;
        return 1;
    }
    if (cv->typeName == "REG") {
        if (isInput && pinIdx == 0) return cv->busWidth;
        if (!isInput && pinIdx == 0) return cv->busWidth;
        return 1;
    }
    if (cv->typeName == "PORT_IN" || cv->typeName == "PORT_OUT") {
        if (pinIdx == 0) return cv->busWidth;
        return 1;
    }
    if (cv->typeName == "NUM_IN") {
        if (!isInput && pinIdx == 0) return cv->busWidth;
        return 1;
    }
    if (cv->typeName == "NUM_DISP") {
        if (isInput && pinIdx == 0) return cv->busWidth;
        return 1;
    }

    return 1;
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

    int srcPortWidth = getAvailablePortWidth(srcCv, src.pinIdx, false);
    int dstPortWidth = getAvailablePortWidth(dstCv, dst.pinIdx, true);
    
    int actualBw = std::min({busWidth, srcPortWidth, dstPortWidth});

    WireView wv;
    wv.id       = nextWireId++;
    wv.busWidth = actualBw;
    wv.src      = src;
    wv.dst      = dst;

    for (int i = 0; i < actualBw; ++i) {
        Driver*   drv = srcCv->comp->getDriver(src.pinIdx + i);
        Receiver* rcv = dstCv->comp->getReceiver(dst.pinIdx + i);
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

// ─── Wire/Junction Removal ────────────────────────────────────────────────────

void Canvas::removeWiresOf(int compId)
{
    std::vector<int> toRemove;
    for (const auto& wv : wires) {
        if ((wv.src.kind == EndpointKind::Component && wv.src.compId == compId) ||
            (wv.dst.kind == EndpointKind::Component && wv.dst.compId == compId)) {
            toRemove.push_back(wv.id);
        }
    }
    for (int id : toRemove) {
        removeWire(id);
    }
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
                        sim->disconnectDriver(cv->comp->getDriver(wv.src.pinIdx + i));
                }
            }
        }
        if (wv.dst.kind == EndpointKind::Component) {
            if (auto* cv = findComp(wv.dst.compId)) {
                if (wv.busWidth <= 1) {
                    sim->disconnectReceiver(cv->comp->getReceiver(wv.dst.pinIdx));
                } else {
                    for (int i = 0; i < wv.busWidth; ++i)
                        sim->disconnectReceiver(cv->comp->getReceiver(wv.dst.pinIdx + i));
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

// ─── Delete Selected ──────────────────────────────────────────────────────────

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
    for (auto& w : wires) {
        if (w.selected) wiresToDelete.push_back(w.id);
        else {
            for (int i = (int)w.waypointSelected.size() - 1; i >= 0; --i) {
                if (w.waypointSelected[i]) {
                    if (i < (int)w.waypoints.size()) {
                        w.waypoints.erase(w.waypoints.begin() + i);
                    }
                    w.waypointSelected.erase(w.waypointSelected.begin() + i);
                }
            }
        }
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
                            for (int i = 0; i < wv.busWidth; ++i) sim->disconnectDriver(cv->comp->getDriver(wv.src.pinIdx + i));
                        }
                    }
                }
                if (wv.dst.kind == EndpointKind::Component) {
                    if (auto* cv = findComp(wv.dst.compId)) {
                        if (wv.busWidth <= 1) {
                            sim->disconnectReceiver(cv->comp->getReceiver(wv.dst.pinIdx));
                        } else {
                            for (int i = 0; i < wv.busWidth; ++i) sim->disconnectReceiver(cv->comp->getReceiver(wv.dst.pinIdx + i));
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
                remainingWires.push_back(std::move(wv));
            }
        }
        wires = std::move(remainingWires);
    }

    cleanupDanglingJunctions();
    selectedId = -1;
    sim->settle();
}

// ─── Lookup Helpers ───────────────────────────────────────────────────────────

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

void Canvas::clear()
{
    clearSelection();

    // Remove all wires first to clean up nets
    while (!wires.empty()) {
        removeWire(wires.back().id);
    }

    junctions.clear();

    // Unregister and remove all components
    while (!comps.empty()) {
        int id = comps.back().id;
        auto it = std::find_if(comps.begin(), comps.end(),
                               [&](const ComponentView& cv){ return cv.id == id; });
        if (it != comps.end()) {
            if (sim) {
                if (auto* clk = dynamic_cast<Clock*>(it->comp.get()))
                    sim->unregisterClock(clk);
                else
                    sim->unregisterComponent(it->comp.get());
            }
            comps.erase(it);
        }
    }

    if (sim) {
        sim->stop();
    }
}

// ─── Hit Testing ──────────────────────────────────────────────────────────────

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
        if (busSide && hasConsolidatedBusDriver(cv)) {
            ImVec2 p = busDriverPos(cv);
            float dx = wp.x - p.x, dy = wp.y - p.y;
            if (dx*dx + dy*dy <= (PIN_RAD+6)*(PIN_RAD+6)) {
                outId = cv.id;
                return 0;
            }
        }
        if (!busSide && (cv.typeName == "BUS_MERGE" || cv.typeName == "REG" || cv.typeName == "PORT_IN" || cv.typeName == "NUM_IN" || cv.typeName == "PORT_OUT")) continue;
        if (cv.typeName == "PORT_OUT") continue;
        for (int i = 0; i < cv.comp->numDrivers(); ++i) {
            int customPortBw = 1;
            if (!isPortGroupStart(cv, false, i, customPortBw)) continue;
            
            // If the port has busWidth > 1, it only hits if busSide is true
            // Wait, does the canvas drawing distinguish bus side for custom components?
            // Actually bus wires connect to bus pins. If customPortBw > 1, it's a bus pin!
            if ((customPortBw > 1) != busSide) continue;

            ImVec2 p = driverPos(cv, i);
            float dx = wp.x - p.x, dy = wp.y - p.y;
            float rad = (customPortBw > 1) ? (PIN_RAD+6) : (PIN_RAD+4);
            if (dx*dx + dy*dy <= rad*rad) {
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
        if (busSide && hasConsolidatedBusReceiver(cv)) {
            ImVec2 p = busReceiverPos(cv);
            float dx = wp.x - p.x, dy = wp.y - p.y;
            if (dx*dx + dy*dy <= (PIN_RAD+6)*(PIN_RAD+6)) {
                outId = cv.id;
                return 0;
            }
        }
        if (!busSide && (cv.typeName == "BUS_SPLIT" || cv.typeName == "PORT_IN" || cv.typeName == "PORT_OUT" || cv.typeName == "NUM_DISP")) continue;
        if (cv.typeName == "PORT_IN") continue;
        for (int i = 0; i < cv.comp->numReceivers(); ++i) {
            int customPortBw = 1;
            if (!isPortGroupStart(cv, true, i, customPortBw)) continue;
            
            if ((customPortBw > 1) != busSide) continue;

            ImVec2 p = receiverPos(cv, i);
            float dx = wp.x - p.x, dy = wp.y - p.y;
            float rad = (customPortBw > 1) ? (PIN_RAD+6) : (PIN_RAD+4);
            if (dx*dx + dy*dy <= rad*rad) {
                outId = cv.id;
                return i;
            }
        }
    }
    outId = -1;
    return -1;
}

bool Canvas::hitRailTap(ImVec2, ImVec2, ImVec2, bool&, float&) const
{
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
        auto pts = routeWire(src, dst, wv.src, wv.dst, wv.waypoints);
        for (size_t i = 1; i < pts.size(); ++i) {
            ImVec2 a = pts[i-1], b = pts[i];
            float minX = std::min(a.x, b.x) - thresh;
            float maxX = std::max(a.x, b.x) + thresh;
            float minY = std::min(a.y, b.y) - thresh;
            float maxY = std::max(a.y, b.y) + thresh;
            if (wp.x < minX || wp.x > maxX || wp.y < minY || wp.y > maxY) continue;

            // Check perpendicular distance to segment for non-axis-aligned segments too
            float segDx = b.x - a.x;
            float segDy = b.y - a.y;
            float segLen2 = segDx * segDx + segDy * segDy;
            if (segLen2 < 0.1f) continue;
            float t = std::clamp(((wp.x - a.x) * segDx + (wp.y - a.y) * segDy) / segLen2, 0.f, 1.f);
            float projX = a.x + t * segDx;
            float projY = a.y + t * segDy;
            float dist2 = (wp.x - projX) * (wp.x - projX) + (wp.y - projY) * (wp.y - projY);
            if (dist2 < thresh * thresh) {
                outWorld = snapToGrid({ wp.x, wp.y });
                return wv.id;
            }
        }
    }
    return -1;
}

int Canvas::hitWaypoint(ImVec2 wp, ImVec2 origin, ImVec2 size, int& outWaypointIdx) const
{
    const float thresh = 8.f / zoom;
    for (const auto& wv : wires) {
        for (int i = 0; i < (int)wv.waypoints.size(); ++i) {
            float dx = wp.x - wv.waypoints[i].x;
            float dy = wp.y - wv.waypoints[i].y;
            if (dx * dx + dy * dy <= thresh * thresh) {
                outWaypointIdx = i;
                return wv.id;
            }
        }
    }
    outWaypointIdx = -1;
    return -1;
}

int Canvas::hitAnyPin(ImVec2 wp, int& outCompId, int& outPinIdx, bool& outIsDriver) const
{
    // Check bus driver pins first
    for (const auto& cv : comps) {
        if (hasConsolidatedBusDriver(cv)) {
            ImVec2 p = busDriverPos(cv);
            float dx = wp.x - p.x, dy = wp.y - p.y;
            if (dx*dx + dy*dy <= (PIN_RAD+6)*(PIN_RAD+6)) {
                outCompId = cv.id;
                outPinIdx = 0;
                outIsDriver = true;
                return cv.id;
            }
        }
    }
    // Check bus receiver pins first
    for (const auto& cv : comps) {
        if (hasConsolidatedBusReceiver(cv)) {
            ImVec2 p = busReceiverPos(cv);
            float dx = wp.x - p.x, dy = wp.y - p.y;
            if (dx*dx + dy*dy <= (PIN_RAD+6)*(PIN_RAD+6)) {
                outCompId = cv.id;
                outPinIdx = 0;
                outIsDriver = false;
                return cv.id;
            }
        }
    }
    // Check individual driver pins
    for (const auto& cv : comps) {
        if (hasConsolidatedBusDriver(cv) || (cv.typeName == "PORT_OUT")) continue;
        for (int i = 0; i < cv.comp->numDrivers(); ++i) {
            int customPortBw = 1;
            if (!isPortGroupStart(cv, false, i, customPortBw)) continue;

            ImVec2 p = driverPos(cv, i);
            float dx = wp.x - p.x, dy = wp.y - p.y;
            float rad = (customPortBw > 1) ? (PIN_RAD+6) : (PIN_RAD+4);
            if (dx*dx + dy*dy <= rad*rad) {
                outCompId = cv.id;
                outPinIdx = i;
                outIsDriver = true;
                return cv.id;
            }
        }
    }
    // Check individual receiver pins
    for (const auto& cv : comps) {
        if (hasConsolidatedBusReceiver(cv) || cv.typeName == "PORT_IN") continue;
        for (int i = 0; i < cv.comp->numReceivers(); ++i) {
            int customPortBw = 1;
            if (!isPortGroupStart(cv, true, i, customPortBw)) continue;

            ImVec2 p = receiverPos(cv, i);
            float dx = wp.x - p.x, dy = wp.y - p.y;
            float rad = (customPortBw > 1) ? (PIN_RAD+6) : (PIN_RAD+4);
            if (dx*dx + dy*dy <= rad*rad) {
                outCompId = cv.id;
                outPinIdx = i;
                outIsDriver = false;
                return cv.id;
            }
        }
    }
    outCompId = -1;
    outPinIdx = -1;
    return -1;
}

// ─── Component Click Handling ─────────────────────────────────────────────────

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
        ni->setValue(static_cast<int>((ni->getValue() + 1) & ni->valueMask()));
        sim->settle();
        return true;
    }
    if (cv->typeName == "PORT_IN" && mouseUp) {
        auto* pi = static_cast<PortIn*>(cv->comp.get());
        pi->testValue = (pi->testValue + 1) & ((1ULL << pi->getBusWidth()) - 1);
        pi->update();
        sim->settle();
        return true;
    }
    return false;
}

void Canvas::handleScrollOnComponent(int compId, float scroll)
{
    auto* cv = findComp(compId);
    if (!cv) return;
    
    if (cv->typeName == "NUM_IN") {
        auto* ni = static_cast<NumericInput*>(cv->comp.get());
        uint64_t mask = ni->valueMask();
        uint64_t v = ni->getValue();
        v = scroll > 0 ? (v + 1) & mask : (v - 1) & mask;
        ni->setValue(static_cast<int>(v));
        sim->settle();
    } else if (cv->typeName == "PORT_IN") {
        auto* pi = static_cast<PortIn*>(cv->comp.get());
        uint64_t v = pi->testValue;
        uint64_t mask = (1ULL << pi->getBusWidth()) - 1;
        v = scroll > 0 ? (v + 1) & mask : (v - 1) & mask;
        pi->testValue = v;
        pi->update();
        sim->settle();
    }
}

// ─── Wire Routing ─────────────────────────────────────────────────────────────

std::vector<ImVec2> Canvas::routeWire(ImVec2 src, ImVec2 dst,
                                       const Endpoint& srcEp, const Endpoint& dstEp,
                                       const std::vector<ImVec2>& waypoints) const
{
    // If user has defined waypoints, use them directly
    if (!waypoints.empty()) {
        std::vector<ImVec2> pts;
        pts.push_back(src);
        for (const auto& wp : waypoints) pts.push_back(wp);
        pts.push_back(dst);
        return pts;
    }

    // Return a straight line if there are no user-defined waypoints
    return { src, dst };
}

// ─── Drawing: Grid ────────────────────────────────────────────────────────────

void Canvas::drawGrid(ImDrawList* dl, ImVec2 origin, ImVec2 size) const
{
    ImVec2 wMin = s2w(origin, origin);
    ImVec2 wMax = s2w({origin.x+size.x, origin.y+size.y}, origin);

    ImU32 col = IM_COL32(40, 40, 52, 100);
    float gridStep = GRID;
    bool drawCoarse = (zoom > 0.8f);
    ImU32 coarseCol = IM_COL32(50, 50, 65, 140);

    float startX = std::floor(wMin.x / gridStep) * gridStep;
    float startY = std::floor(wMin.y / gridStep) * gridStep;

    float dotR = std::max(0.8f, 1.0f * zoom);
    for (float wx = startX; wx <= wMax.x; wx += gridStep) {
        for (float wy = startY; wy <= wMax.y; wy += gridStep) {
            ImVec2 p = w2s({wx, wy}, origin);
            bool isCoarse = (std::fabs(std::fmod(wx, GRID * 5.f)) < 0.1f &&
                             std::fabs(std::fmod(wy, GRID * 5.f)) < 0.1f);
            if (drawCoarse && isCoarse) {
                dl->AddCircleFilled(p, dotR * 1.8f, coarseCol);
            } else {
                dl->AddCircleFilled(p, dotR, col);
            }
        }
    }
}

// ─── Drawing: Rails (removed — power is implicit) ─────────────────────────────

void Canvas::drawRails(ImDrawList*, ImVec2, ImVec2) const
{
}

// ─── Drawing: Pin Labels ──────────────────────────────────────────────────────

const char* Canvas::pinLabel(const std::string& type, bool isInput, int idx, int busWidth) const
{
    if (isInput) {
        auto it = customDefs.find(type);
        if (it != customDefs.end()) {
            int currentPin = 0;
            for (const auto& p : it->second.inPorts) {
                if (idx >= currentPin && idx < currentPin + p.busWidth) {
                    return p.label.c_str();
                }
                currentPin += p.busWidth;
            }
        }
        
        if (type == "NOT" || type == "BUF") {
            if (idx == 0) return "A";
        }
        if (type == "AND" || type == "NAND" || type == "OR" || type == "NOR" || type == "XOR" || type == "XNOR") {
            if (idx == 0) return "A";
            if (idx == 1) return "B";
        }
        if (type == "LED") {
            if (idx == 0) return "A";
        }
        if (type == "NUM_DISP") {
            if (idx == 0) {
                static char buf[16];
                std::snprintf(buf, sizeof(buf), "[%d]", busWidth);
                return buf;
            }
            return "";
        }
        if (type == "RGB_DISP") {
            int cw = busWidth;
            if (cw < 1) cw = 1;
            if (idx % cw != 0) return "";
            int channel = idx / cw;
            if (channel >= 3) return "";
            if (cw == 1) return kRgbChannelNames[channel];
            static char buf[16];
            std::snprintf(buf, sizeof(buf), "%s[%d]", kRgbChannelNames[channel], cw);
            return buf;
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
        if (type == "REG") {
            if (idx == busWidth) return "CLK";
            if (idx == 0) {
                static char buf[16];
                std::snprintf(buf, sizeof(buf), "[%d]", busWidth);
                return buf;
            }
            return "";
        }
    } else {
        auto it = customDefs.find(type);
        if (it != customDefs.end()) {
            int currentPin = 0;
            for (const auto& p : it->second.outPorts) {
                if (idx >= currentPin && idx < currentPin + p.busWidth) {
                    return p.label.c_str();
                }
                currentPin += p.busWidth;
            }
        }
        
        if (type == "BUS_SPLIT") {
            static char buf[8];
            std::snprintf(buf, sizeof(buf), "%d", idx);
            return buf;
        }
        if (type == "NUM_IN") {
            if (idx == 0) {
                static char buf[16];
                std::snprintf(buf, sizeof(buf), "[%d]", busWidth);
                return buf;
            }
            return "";
        }
        if (type == "BUS_MERGE") {
            static char buf[16];
            std::snprintf(buf, sizeof(buf), "[%d]", busWidth);
            return buf;
        }
        if (type == "REG") {
            if (idx == 0) {
                static char buf[16];
                std::snprintf(buf, sizeof(buf), "[%d]", busWidth);
                return buf;
            }
            return "";
        }
    }
    return "";
}

// ─── Drawing: Component ───────────────────────────────────────────────────────

void Canvas::drawComp(ImDrawList* dl, const ComponentView& cv, ImVec2 origin) const
{
    ImVec2 tl = w2s(cv.pos, origin);
    ImVec2 br = w2s({cv.pos.x + cv.size.x, cv.pos.y + cv.size.y}, origin);

    float rounding = 6.f * zoom;
    float fontSize = 13.f * zoom;

    // ── Drop shadow ──
    ImVec2 shadowOff = {2.5f * zoom, 2.5f * zoom};
    dl->AddRectFilled({tl.x + shadowOff.x, tl.y + shadowOff.y},
                      {br.x + shadowOff.x, br.y + shadowOff.y},
                      IM_COL32(0, 0, 0, 60), rounding);

    // ── Body gradient (top lighter, bottom darker) ──
    ImU32 bodyTop = IM_COL32(32, 32, 44, 255);
    ImU32 bodyBot = IM_COL32(22, 22, 30, 255);
    dl->AddRectFilledMultiColor(tl, br, bodyTop, bodyTop, bodyBot, bodyBot);
    // Add rounded corner overlay
    dl->AddRectFilled(tl, br, IM_COL32(0, 0, 0, 0), rounding);

    // ── Border ──
    ImU32 borderCol = cv.selected ? IM_COL32(80, 190, 200, 255)
                                  : IM_COL32(55, 58, 72, 255);
    dl->AddRect(tl, br, borderCol, rounding, 0, cv.selected ? 2.5f : 1.5f);

    // ── RGB color swatch ──
    if (cv.typeName == "RGB_DISP") {
        auto* rgb = static_cast<RGBDisplay*>(cv.comp.get());
        float pad = 8.f * zoom;
        float labelCol = 14.f * zoom;
        ImVec2 innerTL = { tl.x + pad + labelCol, tl.y + pad };
        ImVec2 innerBR = { br.x - pad, br.y - pad };
        float swatchRound = 4.f * zoom;

        if (rgb->hasAmbiguity()) {
            dl->AddRectFilled(innerTL, innerBR, IM_COL32(45, 45, 55, 255), swatchRound);
            if (fontSize >= 8.f) {
                ImVec2 ts = ImGui::CalcTextSize("?");
                float sc = (fontSize * 1.2f) / ImGui::GetFontSize();
                ImVec2 centre = { (innerTL.x + innerBR.x) * .5f, (innerTL.y + innerBR.y) * .5f };
                dl->AddText(ImGui::GetFont(), fontSize * 1.2f,
                            { centre.x - ts.x * sc * .5f, centre.y - ts.y * sc * .5f },
                            IM_COL32(180, 180, 190, 220), "?");
            }
        } else {
            uint32_t col = rgb->getColor();
            dl->AddRectFilled(innerTL, innerBR,
                IM_COL32((col >> 16) & 0xFF, (col >> 8) & 0xFF, col & 0xFF, 255), swatchRound);
        }
        dl->AddRect(innerTL, innerBR, IM_COL32(30, 30, 40, 200), swatchRound, 0, 1.f);

        char hexBuf[16];
        std::snprintf(hexBuf, sizeof(hexBuf), "#%06X",
                      rgb->hasAmbiguity() ? 0u : (rgb->getColor() & 0xFFFFFFu));
        if (fontSize >= 8.f) {
            ImVec2 ts = ImGui::CalcTextSize(hexBuf);
            float sc = (fontSize * 0.65f) / ImGui::GetFontSize();
            dl->AddText(ImGui::GetFont(), fontSize * 0.65f,
                        { innerBR.x - ts.x * sc - 2.f, innerBR.y - ts.y * sc - 2.f },
                        IM_COL32(220, 220, 230, 200), hexBuf);
        }

        float labelSize = fontSize * 0.75f;
        float labelScale = labelSize / ImGui::GetFontSize();
        for (int c = 0; c < 3; ++c) {
            ImVec2 ts = ImGui::CalcTextSize(kRgbChannelNames[c]);
            float y = tl.y + kRgbChannelT[c] * (br.y - tl.y);
            dl->AddText(ImGui::GetFont(), labelSize,
                        { tl.x + pad, y - ts.y * labelScale * 0.5f },
                        kRgbChannelLabelColors[c], kRgbChannelNames[c]);
        }
    }

    // ── Label ──
    if (cv.typeName == "NUM_DISP") {
        auto* nd = static_cast<NumericDisplay*>(cv.comp.get());
        char valBuf[32];
        if (nd->hasAmbiguity())
            std::snprintf(valBuf, sizeof(valBuf), "?");
        else
            std::snprintf(valBuf, sizeof(valBuf), "%llu",
                          static_cast<unsigned long long>(nd->getValue()));
        char hexBuf[24];
        std::snprintf(hexBuf, sizeof(hexBuf), "0x%llX",
                      static_cast<unsigned long long>(nd->hasAmbiguity() ? 0 : nd->getValue()));

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
    } else if (cv.typeName != "RGB_DISP" && fontSize >= 8.f) {
        ImVec2 centre = { (tl.x + br.x) * .5f, (tl.y + br.y) * .5f };
        std::string labelStr = cv.typeName;
        if (cv.typeName == "PORT_IN") {
            if (auto* pi = dynamic_cast<PortIn*>(cv.comp.get())) labelStr = pi->label;
        } else if (cv.typeName == "PORT_OUT") {
            if (auto* po = dynamic_cast<PortOut*>(cv.comp.get())) labelStr = po->label;
        }
        
        ImVec2 ts     = ImGui::CalcTextSize(labelStr.c_str());
        float scale   = fontSize / ImGui::GetFontSize();
        dl->AddText(ImGui::GetFont(), fontSize,
                    { centre.x - ts.x * scale * .5f, centre.y - ts.y * scale * .5f },
                    IM_COL32(220, 225, 240, 255), labelStr.c_str());
    }

    // ── LED glow effect ──
    if (cv.typeName == "LED") {
        auto* led = static_cast<LED*>(cv.comp.get());
        ImVec2 c = { (tl.x+br.x)*.5f, (tl.y+br.y)*.5f };
        float r  = (br.x - tl.x) * .35f;
        if (led->isLit()) {
            // Multi-layered glow
            dl->AddCircleFilled(c, r * 2.0f, IM_COL32(76,185,80,25));
            dl->AddCircleFilled(c, r * 1.5f, IM_COL32(76,185,80,50));
            dl->AddCircleFilled(c, r,        IM_COL32(76,195,80,220));
            dl->AddCircle(c, r, IM_COL32(120,220,120,180), 0, 1.5f);
        } else {
            dl->AddCircleFilled(c, r, IM_COL32(40,45,55,200));
            dl->AddCircle(c, r, IM_COL32(80,85,95,180), 0, 1.5f);
        }
    }

    int bw = componentBusWidth(cv);

    // ── Draw receiver (input) pins ──
    for (int i = 0; i < cv.comp->numReceivers(); ++i) {
        if (cv.typeName == "PORT_IN") break;
        if ((cv.typeName == "BUS_SPLIT" || cv.typeName == "REG" || cv.typeName == "PORT_OUT" || cv.typeName == "NUM_DISP") && i > 0 && i < bw) continue;
        
        int customPortBw = 1;
        if (!isPortGroupStart(cv, true, i, customPortBw)) continue;
        
        ImVec2 tip, edge;
        ImVec2 edgeW;
        if (i == 0 && (cv.typeName == "BUS_SPLIT" || cv.typeName == "REG" || cv.typeName == "PORT_OUT" || cv.typeName == "NUM_DISP")) {
            edgeW = {cv.pos.x, busReceiverPos(cv).y};
            tip  = w2s(busReceiverPos(cv), origin);
            edge = w2s(edgeW, origin);
        } else {
            edgeW = receiverEdgePos(cv, i);
            tip  = w2s(receiverPos(cv, i), origin);
            edge = w2s(edgeW, origin);
        }
        State st  = cv.comp->getReceiver(i)->getState();
        ImU32 col = stateColor(st);
        
        bool isBusDraw = ((i == 0 && (cv.typeName == "BUS_SPLIT" || cv.typeName == "REG" || cv.typeName == "PORT_OUT" || cv.typeName == "NUM_DISP")) || customPortBw > 1);
        float lw  = isBusDraw ? 4.f * zoom : 2.2f * zoom;

        if (isBusDraw) {
            bool anyTrue = false;
            bool anyFalse = false;
            int bw = customPortBw > 1 ? customPortBw : cv.busWidth;
            for (int b = 0; b < bw; ++b) {
                State bs = cv.comp->getReceiver(i + b)->getState();
                if (sim) {
                    if (readAsTrue(bs, *sim))  anyTrue = true;
                    if (readAsFalse(bs, *sim)) anyFalse = true;
                } else {
                    if (bs == State::HIGH) anyTrue = true;
                    if (bs == State::LOW)  anyFalse = true;
                }
            }
            if (anyTrue && !anyFalse) col = IM_COL32(250, 50, 50, 255);
            else if (!anyTrue && anyFalse) col = IM_COL32(50, 150, 250, 255);
            else if (anyTrue && anyFalse) col = IM_COL32(200, 100, 200, 255);
        }

        dl->AddLine(edge, tip, col, lw);

        // Pin circle: filled if connected, hollow if not
        bool connected = cv.comp->getReceiver(i)->isConnected();
        if (connected) {
            dl->AddCircleFilled(tip, PIN_RAD * zoom, col);
        } else {
            dl->AddCircleFilled(tip, PIN_RAD * zoom, IM_COL32(26, 26, 36, 255));
            dl->AddCircle(tip, PIN_RAD * zoom, col, 0, 1.8f * zoom);
        }
 
        const char* lbl = pinLabel(cv.typeName, true, i, bw);
        if (*lbl && fontSize >= 9.f) {
            ImVec2 ts = ImGui::CalcTextSize(lbl);
            float sc = fontSize / ImGui::GetFontSize() * 0.75f;
            ImVec2 textPos;
            if (std::abs(edgeW.x - cv.pos.x) < 0.1f) {
                textPos = { edge.x + 3.f * zoom, edge.y - ts.y * sc * 0.5f };
            } else if (std::abs(edgeW.x - (cv.pos.x + cv.size.x)) < 0.1f) {
                textPos = { edge.x - ts.x * sc - 3.f * zoom, edge.y - ts.y * sc * 0.5f };
            } else if (std::abs(edgeW.y - cv.pos.y) < 0.1f) {
                textPos = { edge.x - ts.x * sc * 0.5f, edge.y + 3.f * zoom };
            } else {
                textPos = { edge.x - ts.x * sc * 0.5f, edge.y - ts.y * sc - 3.f * zoom };
            }
            ImU32 labelCol = IM_COL32(160, 165, 180, 220);
            if (cv.typeName == "RGB_DISP") {
                int channel = i / std::max(1, bw);
                if (channel >= 0 && channel < 3)
                    labelCol = kRgbChannelLabelColors[channel];
            }
            dl->AddText(ImGui::GetFont(), fontSize * .75f, textPos, labelCol, lbl);
        }
    }

    // ── Draw driver (output) pins ──
    for (int i = 0; i < cv.comp->numDrivers(); ++i) {
        if (cv.typeName == "PORT_OUT") break;
        if ((cv.typeName == "BUS_MERGE" || cv.typeName == "REG" || cv.typeName == "PORT_IN" || cv.typeName == "NUM_IN") && i > 0) continue;
        
        int customPortBw = 1;
        if (!isPortGroupStart(cv, false, i, customPortBw)) continue;
        
        ImVec2 tip, edge;
        ImVec2 edgeW;
        if (i == 0 && (cv.typeName == "BUS_MERGE" || cv.typeName == "REG" || cv.typeName == "PORT_IN" || cv.typeName == "NUM_IN")) {
            edgeW = {cv.pos.x + cv.size.x, busDriverPos(cv).y};
            tip  = w2s(busDriverPos(cv), origin);
            edge = w2s(edgeW, origin);
        } else {
            edgeW = driverEdgePos(cv, i);
            tip  = w2s(driverPos(cv, i), origin);
            edge = w2s(edgeW, origin);
        }
        State st  = cv.comp->getDriver(i)->getState();
        ImU32 col = stateColor(st);
        
        bool isBusDraw = ((i == 0 && (cv.typeName == "BUS_MERGE" || cv.typeName == "REG" || cv.typeName == "PORT_IN" || cv.typeName == "NUM_IN")) || customPortBw > 1);
        float lw  = isBusDraw ? 4.f * zoom : 2.2f * zoom;

        if (isBusDraw) {
            bool anyTrue = false;
            bool anyFalse = false;
            int bw = customPortBw > 1 ? customPortBw : cv.busWidth;
            for (int b = 0; b < bw; ++b) {
                State bs = cv.comp->getDriver(i + b)->getState();
                if (sim) {
                    if (readAsTrue(bs, *sim))  anyTrue = true;
                    if (readAsFalse(bs, *sim)) anyFalse = true;
                } else {
                    if (bs == State::HIGH) anyTrue = true;
                    if (bs == State::LOW)  anyFalse = true;
                }
            }
            if (anyTrue && !anyFalse) col = IM_COL32(250, 50, 50, 255);
            else if (!anyTrue && anyFalse) col = IM_COL32(50, 150, 250, 255);
            else if (anyTrue && anyFalse) col = IM_COL32(200, 100, 200, 255);
        }

        dl->AddLine(edge, tip, col, lw);

        // Pin circle: filled if connected, hollow if not
        bool connected = cv.comp->getDriver(i)->isConnected();
        if (connected) {
            dl->AddCircleFilled(tip, PIN_RAD * zoom, col);
        } else {
            dl->AddCircleFilled(tip, PIN_RAD * zoom, IM_COL32(26, 26, 36, 255));
            dl->AddCircle(tip, PIN_RAD * zoom, col, 0, 1.8f * zoom);
        }

        const char* lbl = pinLabel(cv.typeName, false, i, bw);
        if (*lbl && fontSize >= 9.f) {
            ImVec2 ts = ImGui::CalcTextSize(lbl);
            float sc = fontSize / ImGui::GetFontSize() * 0.75f;
            ImVec2 textPos;
            if (std::abs(edgeW.x - cv.pos.x) < 0.1f) {
                textPos = { edge.x + 3.f * zoom, edge.y - ts.y * sc * 0.5f };
            } else if (std::abs(edgeW.x - (cv.pos.x + cv.size.x)) < 0.1f) {
                textPos = { edge.x - ts.x * sc - 3.f * zoom, edge.y - ts.y * sc * 0.5f };
            } else if (std::abs(edgeW.y - cv.pos.y) < 0.1f) {
                textPos = { edge.x - ts.x * sc * 0.5f, edge.y + 3.f * zoom };
            } else {
                textPos = { edge.x - ts.x * sc * 0.5f, edge.y - ts.y * sc - 3.f * zoom };
            }
            dl->AddText(ImGui::GetFont(), fontSize * .75f, textPos,
                        IM_COL32(160,165,180,220), lbl);
        }
    }

    // ── Switch visual ──
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
            on ? IM_COL32(76,185,80,255) : IM_COL32(64,140,230,255));
    }

    // ── Numeric input value ──
    if (cv.typeName == "NUM_IN" && fontSize >= 8.f) {
        auto* ni = static_cast<NumericInput*>(cv.comp.get());
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%llu",
                      static_cast<unsigned long long>(ni->getValue()));
        ImVec2 ts = ImGui::CalcTextSize(buf);
        float sc = fontSize / ImGui::GetFontSize();
        dl->AddText(ImGui::GetFont(), fontSize,
                    { br.x - ts.x * sc - 4.f, tl.y + 3.f },
                    IM_COL32(255, 200, 80, 220), buf);
    }

    // ── Junction component ──
    if (cv.typeName == "JUNCTION") {
        ImVec2 c = { (tl.x + br.x) * .5f, (tl.y + br.y) * .5f };
        dl->AddCircleFilled(c, 6.f * zoom, IM_COL32(200, 200, 100, 255));
    }
}

// ─── Drawing: Junctions ───────────────────────────────────────────────────────

void Canvas::drawJunctions(ImDrawList* dl, ImVec2 origin, ImVec2 /*size*/) const
{
    for (const auto& j : junctions) {
        ImVec2 s = w2s(j.pos, origin);
        State st = j.net ? j.net->getState() : State::FLOATING;
        ImU32 col = stateColor(st);
        dl->AddCircleFilled(s, 5.5f * zoom, col);
        ImU32 borderCol = j.selected ? IM_COL32(80, 190, 200, 255)
                                     : IM_COL32(255, 255, 255, 140);
        dl->AddCircle(s, 5.5f * zoom, borderCol, 0, j.selected ? 2.5f : 1.5f);
    }
}

// ─── Drawing: Wires ───────────────────────────────────────────────────────────

void Canvas::drawBusSlash(ImDrawList* dl, ImVec2 a, ImVec2 b, int width, ImU32 col) const
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.f) return;

    float mx = (a.x + b.x) * 0.5f;
    float my = (a.y + b.y) * 0.5f;
    float ux = dx / len;
    float uy = dy / len;
    float px = -uy;
    float py = ux;

    float half = 9.f * zoom;
    float along = 4.f * zoom;
    ImVec2 c1 = { mx - px * half - ux * along, my - py * half - uy * along };
    ImVec2 c2 = { mx + px * half + ux * along, my + py * half + uy * along };
    dl->AddLine(c1, c2, IM_COL32(240, 240, 245, 230), std::max(2.f, 2.2f * zoom));

    char lbl[12];
    std::snprintf(lbl, sizeof(lbl), "%d", width);
    ImVec2 ts = ImGui::CalcTextSize(lbl);
    float labelSize = 10.f * zoom;
    float sc = labelSize / ImGui::GetFontSize();
    dl->AddText(ImGui::GetFont(), labelSize,
                { mx + px * (half + 4.f * zoom) - ts.x * sc * 0.5f,
                  my + py * (half + 4.f * zoom) - ts.y * sc * 0.5f },
                IM_COL32(220, 220, 225, 220), lbl);
    (void)col;
}

void Canvas::drawAllWires(ImDrawList* dl, ImVec2 origin, ImVec2 size) const
{
    for (const auto& wv : wires) {
        ImVec2 src = endpointPos(wv.src, origin, size);
        ImVec2 dst = endpointPos(wv.dst, origin, size);
        auto pts   = routeWire(src, dst, wv.src, wv.dst, wv.waypoints);

        State st = State::FLOATING;
        if (wv.busWidth > 1 && !wv.busNets.empty())
            st = wv.busNets[0]->getState();
        else if (wv.net)
            st = wv.net->getState();

        ImU32 col = stateColor(st);
        float lw  = wv.busWidth > 1 ? 4.f * zoom : 2.5f * zoom;

        // Selection glow
        if (wv.selected) {
            for (size_t i = 1; i < pts.size(); ++i) {
                dl->AddLine(w2s(pts[i-1], origin), w2s(pts[i], origin),
                            IM_COL32(80, 190, 200, 100), lw + 5.f * zoom);
            }
        }

        // Draw wire segments with rounded corners
        for (size_t i = 1; i < pts.size(); ++i) {
            ImVec2 a = w2s(pts[i-1], origin);
            ImVec2 b = w2s(pts[i], origin);
            dl->AddLine(a, b, col, lw);
        }

        // Rounded corner dots at bend points
        for (size_t i = 1; i + 1 < pts.size(); ++i) {
            ImVec2 p = w2s(pts[i], origin);
            dl->AddCircleFilled(p, lw * 0.5f, col);
        }

        // Source endpoint dot
        dl->AddCircleFilled(w2s(src, origin), 3.5f * zoom, col);

        // Bus slash + width label
        if (wv.busWidth > 1 && pts.size() >= 2) {
            size_t bestSeg = 1;
            float bestLen = 0.f;
            for (size_t i = 1; i < pts.size(); ++i) {
                float dx = pts[i].x - pts[i - 1].x;
                float dy = pts[i].y - pts[i - 1].y;
                float segLen = dx * dx + dy * dy;
                if (segLen > bestLen) {
                    bestLen = segLen;
                    bestSeg = i;
                }
            }
            drawBusSlash(dl,
                         w2s(pts[bestSeg - 1], origin),
                         w2s(pts[bestSeg], origin),
                         wv.busWidth, col);
        }

        // ── Waypoint handles (only when zoomed in enough) ──
        if (zoom > 0.35f && !wv.waypoints.empty()) {
            for (int i = 0; i < (int)wv.waypoints.size(); ++i) {
                ImVec2 p = w2s(wv.waypoints[i], origin);
                float hs = 4.f * zoom; // half-size of handle
                bool wpSelected = (i < (int)wv.waypointSelected.size() && wv.waypointSelected[i]);
                ImU32 handleCol = wpSelected ? IM_COL32(255, 100, 100, 255) : (wv.selected ? IM_COL32(80, 190, 200, 255) : IM_COL32(200, 200, 220, 200));
                // Diamond shape
                ImVec2 pts4[4] = {
                    {p.x, p.y - hs},
                    {p.x + hs, p.y},
                    {p.x, p.y + hs},
                    {p.x - hs, p.y}
                };
                dl->AddConvexPolyFilled(pts4, 4, handleCol);
                dl->AddPolyline(pts4, 4, IM_COL32(255, 255, 255, 180), ImDrawFlags_Closed, 1.2f);
            }
        }
    }
}

void Canvas::drawWireInProgress(ImDrawList* dl, ImVec2 origin, ImVec2 size, ImVec2 mouseSS) const
{
    if (mode != Mode::DrawingWire) return;

    ImVec2 srcW = endpointPos(wireSrc, origin, size);
    ImVec2 dstW = snapToGrid(s2w(mouseSS, origin)); // Snap drawing endpoint
    auto   pts  = routeWire(srcW, dstW, wireSrc, Endpoint{}, currentWireWaypoints);

    ImU32 col = IM_COL32(80, 200, 210, 200);
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
    dl->AddRectFilled(tl, br, IM_COL32(80, 190, 200, 50), 6.f*zoom);
    dl->AddRect(tl, br, IM_COL32(100, 210, 220, 200), 6.f*zoom, 0, 1.5f);
}

// ─── Drawing: Minimap ─────────────────────────────────────────────────────────

void Canvas::drawMinimap(ImDrawList* dl, ImVec2 origin, ImVec2 canvasSize) const
{
    if (comps.empty() && junctions.empty()) return;

    // Find bounding box of all content
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (const auto& cv : comps) {
        minX = std::min(minX, cv.pos.x);
        minY = std::min(minY, cv.pos.y);
        maxX = std::max(maxX, cv.pos.x + cv.size.x);
        maxY = std::max(maxY, cv.pos.y + cv.size.y);
    }
    for (const auto& j : junctions) {
        minX = std::min(minX, j.pos.x);
        minY = std::min(minY, j.pos.y);
        maxX = std::max(maxX, j.pos.x);
        maxY = std::max(maxY, j.pos.y);
    }

    float padding = 50.f;
    minX -= padding; minY -= padding;
    maxX += padding; maxY += padding;

    float contentW = maxX - minX;
    float contentH = maxY - minY;
    if (contentW < 1.f || contentH < 1.f) return;

    // Minimap dimensions
    float mmW = 140.f;
    float mmH = mmW * (contentH / contentW);
    if (mmH > 100.f) { mmH = 100.f; mmW = mmH * (contentW / contentH); }

    float mmX = origin.x + canvasSize.x - mmW - 10.f;
    float mmY = origin.y + canvasSize.y - mmH - 10.f;

    // Background
    dl->AddRectFilled({mmX, mmY}, {mmX + mmW, mmY + mmH}, IM_COL32(10, 10, 14, 200), 4.f);
    dl->AddRect({mmX, mmY}, {mmX + mmW, mmY + mmH}, IM_COL32(60, 65, 80, 180), 4.f);

    auto worldToMm = [&](float wx, float wy) -> ImVec2 {
        return { mmX + (wx - minX) / contentW * mmW,
                 mmY + (wy - minY) / contentH * mmH };
    };

    // Draw components as small rectangles
    for (const auto& cv : comps) {
        ImVec2 tl = worldToMm(cv.pos.x, cv.pos.y);
        ImVec2 br = worldToMm(cv.pos.x + cv.size.x, cv.pos.y + cv.size.y);
        ImU32 c = cv.selected ? IM_COL32(80, 190, 200, 200) : IM_COL32(100, 105, 130, 180);
        dl->AddRectFilled(tl, br, c, 1.f);
    }

    // Draw wires as thin lines
    for (const auto& wv : wires) {
        ImVec2 src = endpointPos(wv.src, origin, canvasSize);
        ImVec2 dst = endpointPos(wv.dst, origin, canvasSize);
        ImVec2 a = worldToMm(src.x, src.y);
        ImVec2 b = worldToMm(dst.x, dst.y);
        dl->AddLine(a, b, IM_COL32(80, 85, 100, 120), 1.f);
    }

    // Draw viewport rectangle
    ImVec2 vpTL = s2w(origin, origin);
    ImVec2 vpBR = s2w({origin.x + canvasSize.x, origin.y + canvasSize.y}, origin);
    ImVec2 vpTLmm = worldToMm(vpTL.x, vpTL.y);
    ImVec2 vpBRmm = worldToMm(vpBR.x, vpBR.y);
    // Clamp to minimap bounds
    vpTLmm.x = std::max(vpTLmm.x, mmX);
    vpTLmm.y = std::max(vpTLmm.y, mmY);
    vpBRmm.x = std::min(vpBRmm.x, mmX + mmW);
    vpBRmm.y = std::min(vpBRmm.y, mmY + mmH);
    dl->AddRectFilled(vpTLmm, vpBRmm, IM_COL32(80, 190, 200, 30));
    dl->AddRect(vpTLmm, vpBRmm, IM_COL32(80, 190, 200, 180), 0.f, 0, 1.5f);
}

// ─── Drawing: Tooltips ────────────────────────────────────────────────────────

void Canvas::drawTooltip(ImVec2 mouseWorld, ImVec2 origin, ImVec2 size) const
{
    if (mode != Mode::Idle) return;
    
    // Check pin hover
    for (const auto& cv : comps) {
        for (int i = 0; i < cv.comp->numDrivers(); ++i) {
            ImVec2 p = driverPos(cv, i);
            float dx = mouseWorld.x - p.x, dy = mouseWorld.y - p.y;
            if (dx*dx + dy*dy <= (PIN_RAD+6)*(PIN_RAD+6)) {
                State st = cv.comp->getDriver(i)->getState();
                const char* label = stateLabel(st, *sim);
                ImGui::SetTooltip("%s [%d] OUT: %s", cv.typeName.c_str(), i, label);
                return;
            }
        }
        for (int i = 0; i < cv.comp->numReceivers(); ++i) {
            ImVec2 p = receiverPos(cv, i);
            float dx = mouseWorld.x - p.x, dy = mouseWorld.y - p.y;
            if (dx*dx + dy*dy <= (PIN_RAD+6)*(PIN_RAD+6)) {
                State st = cv.comp->getReceiver(i)->getState();
                const char* label = stateLabel(st, *sim);
                ImGui::SetTooltip("%s [%d] IN: %s", cv.typeName.c_str(), i, label);
                return;
            }
        }
    }

    // Check wire hover
    ImVec2 hitPos;
    int wId = hitWire(mouseWorld, origin, size, hitPos);
    if (wId >= 0) {
        for (const auto& wv : wires) {
            if (wv.id == wId && wv.net) {
                State st = wv.net->getState();
                const char* label = stateLabel(st, *sim);
                ImGui::SetTooltip("Net: %s\nDrivers: %d  Receivers: %d",
                    label,
                    (int)wv.net->getDrivers().size(),
                    (int)wv.net->getReceivers().size());
                return;
            }
        }
    }
}

// ─── Main Render Loop ─────────────────────────────────────────────────────────

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

    // Background
    dl->AddRectFilled(origin, {origin.x+size.x, origin.y+size.y},
                      IM_COL32(14, 14, 20, 255));

    // ── Smooth zoom interpolation ──
    if (std::fabs(zoom - zoomTarget) > 0.001f) {
        float prevZoom = zoom;
        zoom += (zoomTarget - zoom) * 0.25f; // Lerp factor
        // Adjust pan to keep anchor point stable
        float zr = zoom / prevZoom;
        pan.x = zoomAnchorWorld.x - (zoomAnchorWorld.x - pan.x) * (prevZoom / zoom);
        pan.y = zoomAnchorWorld.y - (zoomAnchorWorld.y - pan.y) * (prevZoom / zoom);
    }

    drawGrid(dl, origin, size);
    drawRails(dl, origin, size);

    // ── Pan (right/middle drag) ──
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

    // ── Zoom (scroll wheel) ──
    if (hovered) {
        float scroll = ImGui::GetIO().MouseWheel;
        if (scroll != 0.f) {
            ImVec2 wBefore = s2w(mousePos, origin);
            int hit = hitComp(wBefore);
            if (hit >= 0)
                handleScrollOnComponent(hit, scroll);
            else {
                // Smooth zoom
                zoomAnchorWorld = wBefore;
                zoomTarget = std::clamp(zoomTarget * (scroll > 0 ? 1.15f : 0.87f), 0.15f, 5.f);
                // Also update zoom immediately a bit for responsiveness
                zoom = std::clamp(zoom * (scroll > 0 ? 1.12f : 0.89f), 0.15f, 5.f);
                ImVec2 wAfter = s2w(mousePos, origin);
                pan.x += wBefore.x - wAfter.x;
                pan.y += wBefore.y - wAfter.y;
            }
        }
    }

    // ── Right-click → context menu / Cancel drawing ──
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        if (mode == Mode::DrawingWire) {
            // Right click while drawing: undo last waypoint or cancel
            if (!currentWireWaypoints.empty()) {
                currentWireWaypoints.pop_back();
            } else {
                mode = Mode::Idle;
                currentWireWaypoints.clear();
            }
        } else {
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
    }

    // ── Left-click handling ──
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImVec2 wp = s2w(mousePos, origin);
        clickStartMouse = wp;

        if (mode == Mode::Placing) {
            placeAt(pendingType, wp, pendingBusWidth);
            if (!ImGui::GetIO().KeyShift) mode = Mode::Idle;

        } else if (mode == Mode::DrawingWire) {
            Endpoint dst;
            int compId, pinIdx;

            // Try to connect to receiver pin (bus or regular)
            pinIdx = hitReceiverPin(wp, compId, true);
            if (pinIdx >= 0) {
                dst.kind = EndpointKind::Component;
                dst.compId = compId;
                dst.pinIdx = pinIdx;
                int bw = 1;
                auto* dstCv = findComp(compId);
                auto* srcCv = wireSrc.kind == EndpointKind::Component
                              ? findComp(wireSrc.compId) : nullptr;
                if (dstCv)
                    bw = std::max(bw, getAvailablePortWidth(dstCv, pinIdx, true));
                if (srcCv && wireSrc.isDriver)
                    bw = std::max(bw, getAvailablePortWidth(srcCv, wireSrc.pinIdx, false));
                completeWire(wireSrc, dst, bw);
                if (!wires.empty() && !currentWireWaypoints.empty()) wires.back().waypoints = currentWireWaypoints;
                currentWireWaypoints.clear();
                mode = Mode::Idle;
            } else {
                pinIdx = hitReceiverPin(wp, compId, false);
                if (pinIdx >= 0) {
                    dst.kind = EndpointKind::Component;
                    dst.compId = compId;
                    dst.pinIdx = pinIdx;
                    completeWire(wireSrc, dst, 1);
                    if (!wires.empty() && !currentWireWaypoints.empty()) wires.back().waypoints = currentWireWaypoints;
                    currentWireWaypoints.clear();
                    mode = Mode::Idle;
                } else {
                    int jId = hitJunction(wp);
                    if (jId >= 0) {
                        dst.kind = EndpointKind::Junction;
                        dst.junctionId = jId;
                        completeWire(wireSrc, dst, 1);
                        if (!wires.empty() && !currentWireWaypoints.empty()) wires.back().waypoints = currentWireWaypoints;
                        currentWireWaypoints.clear();
                        mode = Mode::Idle;
                    } else {
                        // Wire splitting: click on existing wire to connect
                        ImVec2 wirePos;
                        int targetWire = hitWire(wp, origin, size, wirePos);
                        if (targetWire >= 0) {
                            insertJunctionOnWire(targetWire, wirePos);
                            if (!junctions.empty()) {
                                dst.kind = EndpointKind::Junction;
                                dst.junctionId = junctions.back().id;
                                completeWire(wireSrc, dst, 1);
                                if (!wires.empty() && !currentWireWaypoints.empty()) wires.back().waypoints = currentWireWaypoints;
                                currentWireWaypoints.clear();
                                mode = Mode::Idle;
                            }
                        } else {
                            // Clicked empty space while drawing wire! Drop waypoint.
                            currentWireWaypoints.push_back(snapToGrid(wp));
                        }
                    }
                }
            }

        } else {
            // Idle mode clicks
            int compId, pinIdx;

            // Check waypoint first
            {
                int wpIdx;
                int wId = hitWaypoint(wp, origin, size, wpIdx);
                if (wId >= 0) {
                    bool shiftHeld = ImGui::GetIO().KeyShift;
                    if (!shiftHeld) {
                        for (auto& c : comps) c.selected = false;
                        for (auto& jc : junctions) jc.selected = false;
                        for (auto& w : wires) {
                            w.selected = false;
                            for (size_t i = 0; i < w.waypointSelected.size(); ++i) w.waypointSelected[i] = false;
                        }
                    }
                    for (auto& w : wires) {
                        if (w.id == wId) {
                            if (w.waypointSelected.size() != w.waypoints.size()) w.waypointSelected.resize(w.waypoints.size(), false);
                            w.waypointSelected[wpIdx] = shiftHeld ? !w.waypointSelected[wpIdx] : true;
                            break;
                        }
                    }

                    draggingWireId = wId;
                    draggingWaypointIdx = wpIdx;
                    mode = Mode::DraggingWaypoint;
                    goto end_click;
                }
            }

            // Check Pin (driver or receiver) -> PendingPinAction
            bool isDriver;
            if (hitAnyPin(wp, compId, pinIdx, isDriver) >= 0) {
                draggingPinCompId = compId;
                draggingPinIdx = pinIdx;
                draggingPinIsDriver = isDriver;
                mode = Mode::PendingPinAction;
                goto end_click;
            }

            // Check junction
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
                goto end_click;
            }

            // Check comp
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
                goto end_click;
            }

            // Check wire -> PendingWireBranch (if Ctrl is held) or Select
            ImVec2 wirePos;
            int wId = hitWire(wp, origin, size, wirePos);
            if (wId >= 0) {
                if (ImGui::GetIO().KeyCtrl) {
                    pendingWireBranchId = wId;
                    pendingWireBranchPos = wirePos;
                    mode = Mode::PendingWireBranch;
                } else {
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
                    mode = Mode::Idle;
                }
                goto end_click;
            }

            // Empty space → start region selection
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
    end_click:;

    // ── Pending actions motion ──
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f)) {
        if (mode == Mode::PendingPinAction) {
            mode = Mode::DraggingPin;
        } else if (mode == Mode::PendingWireBranch) {
            // Cancel branch if dragging, select the wire instead
            bool shiftHeld = ImGui::GetIO().KeyShift;
            if (!shiftHeld) {
                for (auto& c : comps) c.selected = false;
                for (auto& jc : junctions) jc.selected = false;
                for (auto& w : wires) {
                    if (w.id != pendingWireBranchId) w.selected = false;
                    w.waypointSelected.assign(w.waypointSelected.size(), false);
                }
                selectedId = -1;
            }
            for (auto& w : wires) {
                if (w.id == pendingWireBranchId) {
                    w.selected = shiftHeld ? !w.selected : true;
                    break;
                }
            }
            mode = Mode::Idle;
        }
    }

    // ── Pending actions release (Click-to-route) ──
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (mode == Mode::PendingPinAction) {
            // Clicked and released without moving -> route wire
            wireSrc.kind = EndpointKind::Component;
            wireSrc.compId = draggingPinCompId;
            wireSrc.pinIdx = draggingPinIdx;
            wireSrc.isDriver = draggingPinIsDriver;
            currentWireWaypoints.clear();
            mode = Mode::DrawingWire;
        } else if (mode == Mode::PendingWireBranch) {
            // Clicked and released wire without moving -> branch
            insertJunctionOnWire(pendingWireBranchId, pendingWireBranchPos);
            if (!junctions.empty()) {
                wireSrc.kind = EndpointKind::Junction;
                wireSrc.junctionId = junctions.back().id;
                currentWireWaypoints.clear();
                mode = Mode::DrawingWire;
            } else {
                mode = Mode::Idle;
            }
        }
    }

    // ── Double-click to add waypoint on wire ──
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        ImVec2 wp = s2w(mousePos, origin);
        ImVec2 wirePos;
        int wId = hitWire(wp, origin, size, wirePos);
        if (wId >= 0) {
            // Find the wire and add a waypoint
            for (auto& wv : wires) {
                if (wv.id == wId) {
                    ImVec2 newWp = snapToGrid(wp);
                    if (wv.waypoints.empty()) {
                        // Initialize waypoints from auto-route first
                        ImVec2 src = endpointPos(wv.src, origin, size);
                        ImVec2 dst = endpointPos(wv.dst, origin, size);
                        auto autopts = routeWire(src, dst, wv.src, wv.dst);
                        // Add intermediate points as waypoints (skip first and last which are endpoints)
                        for (size_t i = 1; i + 1 < autopts.size(); ++i) {
                            wv.waypoints.push_back(autopts[i]);
                        }
                    }
                    // Insert the new waypoint at the right position
                    // Find which segment the click is closest to
                    ImVec2 src = endpointPos(wv.src, origin, size);
                    ImVec2 dst = endpointPos(wv.dst, origin, size);
                    std::vector<ImVec2> allPts;
                    allPts.push_back(src);
                    for (auto& w : wv.waypoints) allPts.push_back(w);
                    allPts.push_back(dst);

                    float bestDist = 1e9f;
                    int bestSeg = 0;
                    for (size_t i = 1; i < allPts.size(); ++i) {
                        ImVec2 a = allPts[i-1], b = allPts[i];
                        float segDx = b.x - a.x, segDy = b.y - a.y;
                        float segLen2 = segDx*segDx + segDy*segDy;
                        if (segLen2 < 0.1f) continue;
                        float t = std::clamp(((newWp.x-a.x)*segDx+(newWp.y-a.y)*segDy)/segLen2, 0.f, 1.f);
                        float px = a.x + t*segDx, py = a.y + t*segDy;
                        float d = (newWp.x-px)*(newWp.x-px)+(newWp.y-py)*(newWp.y-py);
                        if (d < bestDist) { bestDist = d; bestSeg = (int)i - 1; }
                    }
                    wv.waypoints.insert(wv.waypoints.begin() + bestSeg, newWp);
                    if (wv.waypointSelected.size() != wv.waypoints.size() - 1) wv.waypointSelected.resize(wv.waypoints.size() - 1, false);
                    wv.waypointSelected.insert(wv.waypointSelected.begin() + bestSeg, false);
                    break;
                }
            }
        }
    }

    // ── Region selection release ──
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

    // ── Button press release ──
    if (mode == Mode::PressingButton && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (pressedButtonId >= 0)
            tryHandleComponentClick(pressedButtonId, false, true);
        pressedButtonId = -1;
        mode = Mode::Idle;
    }

    // ── Component drag release ──
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

    // ── Component drag motion ──
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

    // ── Pin dragging ──
    if (mode == Mode::DraggingPin && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2 wp = s2w(mousePos, origin);
        auto* cv = findComp(draggingPinCompId);
        if (cv) {
            PinLayout newLayout = projectOntoPerimeter(*cv, wp);
            if (draggingPinIsDriver) {
                if (draggingPinIdx >= 0 && draggingPinIdx < (int)cv->driverLayout.size())
                    cv->driverLayout[draggingPinIdx] = newLayout;
            } else {
                if (draggingPinIdx >= 0 && draggingPinIdx < (int)cv->receiverLayout.size())
                    cv->receiverLayout[draggingPinIdx] = newLayout;
            }
        }
    }
    if (mode == Mode::DraggingPin && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        mode = Mode::Idle;
    }

    // ── Waypoint dragging ──
    if (mode == Mode::DraggingWaypoint && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2 wp = s2w(mousePos, origin);
        for (auto& wv : wires) {
            if (wv.id == draggingWireId && draggingWaypointIdx >= 0 &&
                draggingWaypointIdx < (int)wv.waypoints.size()) {
                wv.waypoints[draggingWaypointIdx] = snapToGrid(wp);
                break;
            }
        }
    }
    if (mode == Mode::DraggingWaypoint && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        mode = Mode::Idle;
    }

    // ── Keyboard shortcuts ──
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        mode = Mode::Idle;
        for (auto& cv : comps) cv.selected = false;
        for (auto& j : junctions) j.selected = false;
        for (auto& w : wires) w.selected = false;
        selectedId = -1;
        pressedButtonId = -1;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) deleteSelected();

    // ── Draw everything ──
    drawAllWires(dl, origin, size);
    drawJunctions(dl, origin, size);
    for (const auto& cv : comps) drawComp(dl, cv, origin);
    drawWireInProgress(dl, origin, size, mousePos);
    drawPlacementGhost(dl, origin, mousePos);
    drawMinimap(dl, origin, size);

    // ── Selection rectangle ──
    if (mode == Mode::SelectingRegion) {
        ImVec2 wp = s2w(mousePos, origin);
        ImVec2 tl = w2s({std::min(clickStartMouse.x, wp.x), std::min(clickStartMouse.y, wp.y)}, origin);
        ImVec2 br = w2s({std::max(clickStartMouse.x, wp.x), std::max(clickStartMouse.y, wp.y)}, origin);
        dl->AddRectFilled(tl, br, IM_COL32(80, 190, 200, 25), 0.f);
        dl->AddRect(tl, br, IM_COL32(80, 190, 200, 150), 0.f, 0, 1.5f);
    }

    // ── Tooltips (must be after drawing, before context menu) ──
    if (hovered && mode == Mode::Idle) {
        ImVec2 wp = s2w(mousePos, origin);
        drawTooltip(wp, origin, size);
    }

    // ── Context menu ──
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
                                    for (int i = 0; i < wv.busWidth; ++i) sim->disconnectDriver(cv->comp->getDriver(wv.src.pinIdx + i));
                                }
                            }
                        }
                        if (wv.dst.kind == EndpointKind::Component) {
                            if (auto* cv = findComp(wv.dst.compId)) {
                                if (wv.busWidth <= 1) {
                                    sim->disconnectReceiver(cv->comp->getReceiver(wv.dst.pinIdx));
                                } else {
                                    for (int i = 0; i < wv.busWidth; ++i) sim->disconnectReceiver(cv->comp->getReceiver(wv.dst.pinIdx + i));
                                }
                            }
                        }
                    } else {
                        remainingWires.push_back(std::move(wv));
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
            if (ImGui::MenuItem("Add Corner")) {
                // Add a waypoint at the right-click position
                for (auto& wv : wires) {
                    if (wv.id == rightClickedWireId) {
                        ImVec2 newWp = snapToGrid(rightClickedWireJunctionPos);
                        if (wv.waypoints.empty()) {
                            ImVec2 src = endpointPos(wv.src, origin, size);
                            ImVec2 dst = endpointPos(wv.dst, origin, size);
                            auto autopts = routeWire(src, dst, wv.src, wv.dst);
                            for (size_t i = 1; i + 1 < autopts.size(); ++i) {
                                wv.waypoints.push_back(autopts[i]);
                            }
                        }
                        // Insert at best position
                        ImVec2 src = endpointPos(wv.src, origin, size);
                        ImVec2 dst = endpointPos(wv.dst, origin, size);
                        std::vector<ImVec2> allPts;
                        allPts.push_back(src);
                        for (auto& w : wv.waypoints) allPts.push_back(w);
                        allPts.push_back(dst);

                        float bestDist = 1e9f;
                        int bestSeg = 0;
                        for (size_t i = 1; i < allPts.size(); ++i) {
                            ImVec2 a = allPts[i-1], b = allPts[i];
                            float segDx = b.x-a.x, segDy = b.y-a.y;
                            float segLen2 = segDx*segDx+segDy*segDy;
                            if (segLen2 < 0.1f) continue;
                            float t = std::clamp(((newWp.x-a.x)*segDx+(newWp.y-a.y)*segDy)/segLen2, 0.f, 1.f);
                            float px = a.x+t*segDx, py = a.y+t*segDy;
                            float d = (newWp.x-px)*(newWp.x-px)+(newWp.y-py)*(newWp.y-py);
                            if (d < bestDist) { bestDist = d; bestSeg = (int)i-1; }
                        }
                        wv.waypoints.insert(wv.waypoints.begin() + bestSeg, newWp);
                        break;
                    }
                }
            }
            // Check if there are waypoints near the right-click position
            for (auto& wv : wires) {
                if (wv.id == rightClickedWireId && !wv.waypoints.empty()) {
                    float thresh = 10.f / zoom;
                    for (int i = 0; i < (int)wv.waypoints.size(); ++i) {
                        float dx = rightClickedWireJunctionPos.x - wv.waypoints[i].x;
                        float dy = rightClickedWireJunctionPos.y - wv.waypoints[i].y;
                        if (dx*dx + dy*dy < thresh*thresh) {
                            if (ImGui::MenuItem("Remove Corner")) {
                                wv.waypoints.erase(wv.waypoints.begin() + i);
                            }
                            break;
                        }
                    }
                    break;
                }
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

    // ── Cursor feedback ──
    if (mode == Mode::Placing || mode == Mode::DrawingWire || mode == Mode::SelectingRegion)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    else if (mode == Mode::DraggingPin)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    else if (mode == Mode::DraggingWaypoint)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

    dl->PopClipRect();
}
