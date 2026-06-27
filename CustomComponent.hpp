#pragma once

#include "Components.hpp"
#include "json.hpp"
#include "Simulator.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>

class RGBDisplay;

struct CustomPortDef {
    int internalCompId;
    std::string label;
    int busWidth;
    int side = 0; // 0=Left, 1=Top, 2=Right, 3=Bottom
    float t = 0.5f; // parametric position along edge (0.0–1.0)
    int order = 0;  // legacy; used when t was not saved
};

struct ScreenPixelDef {
    int internalCompId = -1;
    int col = 0;
    int row = 0;
};

struct ScreenDef {
    float x = 0.08f;
    float y = 0.12f;
    float w = 0.84f;
    float h = 0.76f;
    int cols = 1;
    int rows = 1;
    std::vector<ScreenPixelDef> pixels;
};

struct ResolvedScreenPixel {
    RGBDisplay* display = nullptr;
    int col = 0;
    int row = 0;
};

struct ResolvedScreen {
    ScreenDef layout;
    std::vector<ResolvedScreenPixel> pixels;
};

inline std::optional<ScreenDef> screenDefFromJson(const nlohmann::json& j)
{
    if (!j.contains("screen") || !j["screen"].is_object())
        return std::nullopt;

    const auto& js = j["screen"];
    ScreenDef s;
    s.x = js.value("x", 0.08f);
    s.y = js.value("y", 0.12f);
    s.w = js.value("w", 0.84f);
    s.h = js.value("h", 0.76f);
    s.cols = js.value("cols", 1);
    s.rows = js.value("rows", 1);

    if (js.contains("pixels") && js["pixels"].is_array()) {
        for (const auto& jp : js["pixels"]) {
            ScreenPixelDef p;
            p.internalCompId = jp.value("internalCompId", -1);
            p.col = jp.value("col", 0);
            p.row = jp.value("row", 0);
            s.pixels.push_back(p);
        }
    }
    return s;
}

inline nlohmann::json screenDefToJson(const ScreenDef& s)
{
    nlohmann::json js;
    js["x"] = s.x;
    js["y"] = s.y;
    js["w"] = s.w;
    js["h"] = s.h;
    js["cols"] = s.cols;
    js["rows"] = s.rows;
    js["pixels"] = nlohmann::json::array();
    for (const auto& p : s.pixels) {
        nlohmann::json jp;
        jp["internalCompId"] = p.internalCompId;
        jp["col"] = p.col;
        jp["row"] = p.row;
        js["pixels"].push_back(jp);
    }
    return js;
}

struct CustomComponentDef {
    std::string typeName;
    float width;
    float height;
    std::string canvasJson;
    std::vector<CustomPortDef> inPorts;
    std::vector<CustomPortDef> outPorts;
    std::optional<ScreenDef> screen;
};

class CustomComponent : public Component
{
public:
    CustomComponent(const CustomComponentDef& def);
    ~CustomComponent() override;

    void update() override;

    // We don't override pin getters; we simply populate the base class 'receivers' and 'drivers' vectors.
    
    const CustomComponentDef& getDef() const { return def; }
    const ResolvedScreen* getResolvedScreen() const;

    // When the canvas adds this custom component to the simulator, 
    // we need to register all internal components and nets instead!
    void registerInternals(Simulator* sim,
                           const std::unordered_map<std::string, CustomComponentDef>& customDefs);
    void unregisterInternals(Simulator* sim);
    
    void setSimulator(Simulator* s) override;

private:
    CustomComponentDef def;

    // We instantiate our own internal components to flatten into the main simulator
    std::vector<std::unique_ptr<Component>> internalComps;
    std::vector<Net*> rawInternalNets;
    std::optional<ResolvedScreen> resolvedScreen;
};
