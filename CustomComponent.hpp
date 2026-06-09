#pragma once

#include "Components.hpp"
#include "json.hpp"
#include "Simulator.hpp"
#include <string>
#include <vector>
#include <memory>

struct CustomPortDef {
    int internalCompId;
    std::string label;
    int busWidth;
    int side; // 0=Left, 1=Top, 2=Right, 3=Bottom
    int order;
};

struct CustomComponentDef {
    std::string typeName;
    float width;
    float height;
    std::string canvasJson;
    std::vector<CustomPortDef> inPorts;
    std::vector<CustomPortDef> outPorts;
};

class CustomComponent : public Component
{
public:
    CustomComponent(const CustomComponentDef& def);
    ~CustomComponent() override;

    void update() override;

    // We don't override pin getters; we simply populate the base class 'receivers' and 'drivers' vectors.
    
    const CustomComponentDef& getDef() const { return def; }

    // When the canvas adds this custom component to the simulator, 
    // we need to register all internal components and nets instead!
    void registerInternals(Simulator* sim);
    void unregisterInternals(Simulator* sim);

private:
    CustomComponentDef def;
    
    // We instantiate our own internal components to flatten into the main simulator
    std::vector<std::unique_ptr<Component>> internalComps;
    std::vector<Net*> rawInternalNets;
};
