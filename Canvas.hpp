#pragma once
#include <vector>
#include <string>
#include <memory>

#include "imgui.h"
#include "Simulator.hpp"
#include "Components.hpp"

// ─── Canvas ───────────────────────────────────────────────────────────────────

class Canvas
{
public:
    explicit Canvas(Simulator* sim);
    ~Canvas();

    void render();

    void beginPlacement(const std::string& typeName, int busWidth = 1);

    Component*         getSelectedComponent() const;
    const std::string& getSelectedTypeName()  const;

    bool hasSelection() const;
    int  getSelectedComponentCount() const;
    int  getSelectedWireCount() const;
    int  getSelectedJunctionCount() const;
    void clearSelection();

    void deleteSelected();
    void settle() { if (sim) sim->settle(); }

private:
    enum class EndpointKind { Component, Rail, Junction };

    struct Endpoint {
        EndpointKind kind = EndpointKind::Component;
        int  compId    = -1;
        int  pinIdx    = -1;
        bool isDriver  = false;
        bool railIsVdd = true;
        float railX    = 0.f;
        int  junctionId = -1;
    };

    struct ComponentView {
        int         id;
        std::string typeName;
        std::unique_ptr<Component> comp;
        ImVec2      pos;
        ImVec2      size;
        bool        selected = false;
        int         busWidth = 1;
    };

    struct JunctionView {
        int    id;
        Net*   net = nullptr;
        ImVec2 pos;
        bool   selected = false;
    };

    struct WireView {
        int   id;
        int   busWidth = 1;
        Net*  net      = nullptr;
        std::vector<Net*> busNets;
        Endpoint src;
        Endpoint dst;
        bool  selected = false;
    };

    enum class Mode {
        Idle, Placing, DraggingComp, DrawingWire, PressingButton, SelectingRegion
    };

    Simulator*                 sim;
    std::vector<ComponentView> comps;
    std::vector<JunctionView>  junctions;
    std::vector<WireView>      wires;

    std::vector<std::pair<int, ImVec2>> dragStartComps;
    std::vector<std::pair<int, ImVec2>> dragStartJunctions;

    ImVec2  pan      = {0.f, 0.f};
    float   zoom     = 1.f;

    Mode        mode        = Mode::Idle;
    std::string pendingType;
    int         pendingBusWidth = 4;
    int         selectedId   = -1;
    int         rightClickedCompId = -1;
    int         rightClickedJunctionId = -1;
    int         rightClickedWireId = -1;
    ImVec2      rightClickedWireJunctionPos = {0.f, 0.f};

    ImVec2 dragStartMouse;
    ImVec2 dragStartPos;
    ImVec2 clickStartMouse;
    int    pressedButtonId = -1;

    Endpoint wireSrc;

    int nextId         = 1;
    int nextWireId     = 1;
    int nextJunctionId = 1;

    static constexpr float GRID      = 20.f;
    static constexpr float PIN_LEN   = 18.f;
    static constexpr float PIN_RAD   = 6.f;
    static constexpr float COMP_W    = 90.f;
    static constexpr float PIN_SPACE = 28.f;
    static constexpr float SNAP      = 20.f;
    static constexpr float RAIL_BAND = 24.f;
    static constexpr float CLICK_THRESH = 4.f;

    ImVec2 w2s(ImVec2 world, ImVec2 origin) const;
    ImVec2 s2w(ImVec2 screen, ImVec2 origin) const;
    ImVec2 snapToGrid(ImVec2 w) const;

    ImVec2 driverPos(const ComponentView& cv, int idx) const;
    ImVec2 receiverPos(const ComponentView& cv, int idx) const;
    ImVec2 receiverEdgePos(const ComponentView& cv, int idx) const;
    ImVec2 busDriverPos(const ComponentView& cv) const;
    ImVec2 busReceiverPos(const ComponentView& cv) const;
    ImVec2 endpointPos(const Endpoint& ep, ImVec2 origin, ImVec2 canvasSize) const;
    ImVec2 railTapWorldX(float worldX) const;
    float  railScreenY(bool vdd, ImVec2 origin) const;

    int hitComp(ImVec2 wp) const;
    int hitDriverPin(ImVec2 wp, int& outId, bool busSide = false) const;
    int hitReceiverPin(ImVec2 wp, int& outId, bool busSide = false) const;
    bool hitRailTap(ImVec2 wp, ImVec2 origin, ImVec2 size, bool& outVdd, float& outWorldX) const;
    int  hitJunction(ImVec2 wp) const;
    int  hitWire(ImVec2 wp, ImVec2 origin, ImVec2 size, ImVec2& outWorld) const;

    void drawGrid(ImDrawList* dl, ImVec2 origin, ImVec2 size) const;
    void drawRails(ImDrawList* dl, ImVec2 origin, ImVec2 size) const;
    void drawComp(ImDrawList* dl, const ComponentView& cv, ImVec2 origin) const;
    void drawJunctions(ImDrawList* dl, ImVec2 origin, ImVec2 size) const;
    void drawAllWires(ImDrawList* dl, ImVec2 origin, ImVec2 size) const;
    void drawWireInProgress(ImDrawList* dl, ImVec2 origin, ImVec2 size, ImVec2 mouseSS) const;
    void drawPlacementGhost(ImDrawList* dl, ImVec2 origin, ImVec2 mouseSS) const;

    std::vector<ImVec2> routeWire(ImVec2 src, ImVec2 dst, const Endpoint& srcEp, const Endpoint& dstEp) const;
    ImU32 stateColor(State s) const;
    ImVec2 railEndpointWorld(bool isVdd, float worldX, ImVec2 origin, ImVec2 canvasSize) const;

    ComponentView makeView(const std::string& type, ImVec2 worldPos, int busWidth = 1);
    std::unique_ptr<Component> makeComponent(const std::string& type, int busWidth = 1);
    void placeAt(const std::string& type, ImVec2 worldPos, int busWidth = 1);

    static ImVec2 getComponentSize(const std::string& type, int busWidth);

    void completeWire(const Endpoint& src, const Endpoint& dst, int busWidth = 1);
    void completeWireSingle(Driver* drv, Receiver* rcv, const Endpoint& src, const Endpoint& dst);
    void removeWiresOf(int compId);
    void removeWire(int wireId);
    void removeJunction(int junctionId);
    void insertJunctionOnWire(int wireId, ImVec2 worldPos);
    void cleanupDanglingJunctions();

    bool tryHandleComponentClick(int compId, bool mouseDown, bool mouseUp);
    void handleScrollOnComponent(int compId, float scroll);

    ComponentView* findComp(int id);
    const ComponentView* findComp(int id) const;
    JunctionView* findJunction(int id);
    const JunctionView* findJunction(int id) const;

    bool isBusComponent(const std::string& type) const;
    int  componentBusWidth(const ComponentView& cv) const;
};
