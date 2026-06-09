#pragma once
#include <vector>
#include <string>
#include <memory>

#include "imgui.h"
#include "Simulator.hpp"
#include "Components.hpp"
#include "CustomComponent.hpp"
#include <unordered_map>

// ─── Canvas ───────────────────────────────────────────────────────────────────

class Canvas
{
public:
    explicit Canvas(Simulator* sim);
    ~Canvas();

    void render();

    void beginPlacement(const std::string& typeName, int busWidth = 1);
    
    // Custom component definitions
    std::unordered_map<std::string, CustomComponentDef> customDefs;

    Component*         getSelectedComponent() const;
    const std::string& getSelectedTypeName()  const;

    bool hasSelection() const;
    int  getSelectedComponentCount() const;
    int  getSelectedWireCount() const;
    int  getSelectedJunctionCount() const;
    void clearSelection();

    void deleteSelected();
    void settle() { if (sim) sim->settle(); }

    std::string serialize() const;
    void deserialize(const std::string& data);

    const auto& getComps() const { return comps; }

private:
    friend class CustomComponent;
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

    // ── Pin Layout ─────────────────────────────────────────────────────────
    // Defines where a pin sits on its owning component's perimeter.
    // side: 0=Left, 1=Top, 2=Right, 3=Bottom
    // t:    parametric position along that edge (0.0 = start, 1.0 = end)
    struct PinLayout {
        int   side = 0;
        float t    = 0.5f;
    };

    struct ComponentView {
        int         id;
        std::string typeName;
        std::unique_ptr<Component> comp;
        ImVec2      pos;
        ImVec2      size;
        bool        selected = false;
        int         busWidth = 1;

        // Per-pin layout: position on the component perimeter
        std::vector<PinLayout> receiverLayout;
        std::vector<PinLayout> driverLayout;
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

        // User-editable bend points (world coords).
        // Empty = use auto Manhattan routing.
        std::vector<ImVec2> waypoints;
        std::vector<bool> waypointSelected;
    };

    enum class Mode {
        Idle, Placing, DraggingComp, DrawingWire, PressingButton,
        SelectingRegion,
        DraggingPin,       // Dragging a pin around its component edge
        DraggingWaypoint,  // Dragging a wire bend point
        PendingPinAction,  // Mouse down on a pin, deciding between drag and route
        PendingWireBranch  // Mouse down on a wire, deciding between select and branch
    };

    Simulator*                 sim;
    std::vector<ComponentView> comps;
    std::vector<JunctionView>  junctions;
    std::vector<WireView>      wires;

    std::vector<std::pair<int, ImVec2>> dragStartComps;
    std::vector<std::pair<int, ImVec2>> dragStartJunctions;

    ImVec2  pan      = {0.f, 0.f};
    float   zoom     = 1.f;

    // Smooth zoom interpolation
    float   zoomTarget = 1.f;
    ImVec2  zoomAnchorWorld = {0.f, 0.f};

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
    std::vector<ImVec2> currentWireWaypoints;

    // Pin dragging / routing state
    int    draggingPinCompId    = -1;
    int    draggingPinIdx       = -1;
    bool   draggingPinIsDriver  = false;

    // Waypoint dragging state
    int    draggingWireId       = -1;
    int    draggingWaypointIdx  = -1;

    // Wire branching state
    int    pendingWireBranchId  = -1;
    ImVec2 pendingWireBranchPos = {0.f, 0.f};

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

    // Pin positioning (now reads from PinLayout)
    ImVec2 pinWorldPos(const ComponentView& cv, const PinLayout& pl, bool isEdge) const;
    ImVec2 getCustomPinPos(const ComponentView& cv, int side, int totalOnSide, int idxOnSide, bool isEdge) const;
    ImVec2 driverPos(const ComponentView& cv, int idx) const;
    ImVec2 driverEdgePos(const ComponentView& cv, int idx) const;
    ImVec2 receiverPos(const ComponentView& cv, int idx) const;
    ImVec2 receiverEdgePos(const ComponentView& cv, int idx) const;
    ImVec2 busDriverPos(const ComponentView& cv) const;
    ImVec2 busReceiverPos(const ComponentView& cv) const;
    ImVec2 endpointPos(const Endpoint& ep, ImVec2 origin, ImVec2 canvasSize) const;
    ImVec2 railTapWorldX(float worldX) const;
    float  railScreenY(bool vdd, ImVec2 origin) const;
    const char* pinLabel(const std::string& type, bool isInput, int idx, int busWidth) const;

    // Initialize default pin layouts for a component
    void initPinLayouts(ComponentView& cv);

    // Project a world point onto the component perimeter → (side, t)
    PinLayout projectOntoPerimeter(const ComponentView& cv, ImVec2 worldPt) const;

    // Hit testing
    int hitComp(ImVec2 wp) const;
    int hitDriverPin(ImVec2 wp, int& outId, bool busSide = false) const;
    int hitReceiverPin(ImVec2 wp, int& outId, bool busSide = false) const;
    bool hitRailTap(ImVec2 wp, ImVec2 origin, ImVec2 size, bool& outVdd, float& outWorldX) const;
    int  hitJunction(ImVec2 wp) const;
    int  hitWire(ImVec2 wp, ImVec2 origin, ImVec2 size, ImVec2& outWorld) const;
    int  hitWaypoint(ImVec2 wp, ImVec2 origin, ImVec2 size, int& outWaypointIdx) const;
    int  hitAnyPin(ImVec2 wp, int& outCompId, int& outPinIdx, bool& outIsDriver) const;

    // Drawing
    void drawGrid(ImDrawList* dl, ImVec2 origin, ImVec2 size) const;
    void drawRails(ImDrawList* dl, ImVec2 origin, ImVec2 size) const;
    void drawComp(ImDrawList* dl, const ComponentView& cv, ImVec2 origin) const;
    void drawJunctions(ImDrawList* dl, ImVec2 origin, ImVec2 size) const;
    void drawAllWires(ImDrawList* dl, ImVec2 origin, ImVec2 size) const;
    void drawWireInProgress(ImDrawList* dl, ImVec2 origin, ImVec2 size, ImVec2 mouseSS) const;
    void drawPlacementGhost(ImDrawList* dl, ImVec2 origin, ImVec2 mouseSS) const;
    void drawMinimap(ImDrawList* dl, ImVec2 origin, ImVec2 size) const;
    void drawTooltip(ImVec2 mouseWorld, ImVec2 origin, ImVec2 size) const;

    std::vector<ImVec2> routeWire(ImVec2 src, ImVec2 dst, const Endpoint& srcEp, const Endpoint& dstEp,
                                  const std::vector<ImVec2>& waypoints = {}) const;
    ImU32 stateColor(State s) const;
    ImVec2 railEndpointWorld(bool isVdd, float worldX, ImVec2 origin, ImVec2 canvasSize) const;

    ComponentView makeView(const std::string& type, ImVec2 worldPos, int busWidth = 1);
    std::unique_ptr<Component> makeComponent(const std::string& type, int busWidth = 1);
    void placeAt(const std::string& type, ImVec2 worldPos, int busWidth = 1);

    ImVec2 getComponentSize(const std::string& type, int busWidth) const;

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
