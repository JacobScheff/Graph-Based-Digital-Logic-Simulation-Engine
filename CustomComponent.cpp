#include "CustomComponent.hpp"
#include "Canvas.hpp" // For temporary parsing
#include "IO.hpp"
#include <iostream>

CustomComponent::CustomComponent(const CustomComponentDef& d)
    : Component(d.typeName, 0, 0), def(d)
{
    // The base Component class receives 0,0 for now because we will dynamically populate the vectors
    // Wait, Component base class allocates vectors based on numReceivers and numDrivers.
    // If we override getReceiver and getDriver, we don't even use the base vectors.
}

CustomComponent::~CustomComponent()
{
    if (getSimulator()) {
        unregisterInternals(getSimulator());
    }
    
    // The simulator unregistration and net destruction will be handled when 
    // unregisterInternals is called (usually by App/Canvas before deletion).
    // We MUST clear these vectors so ~Component doesn't try to delete them.
    this->receivers.clear();
    this->drivers.clear();
}

void CustomComponent::update()
{
    // No-op. The actual logic is performed by the internal components!
}

void CustomComponent::setSimulator(Simulator* s)
{
    if (s == nullptr && getSimulator() != nullptr) {
        unregisterInternals(getSimulator());
    }
    Component::setSimulator(s);
}

const ResolvedScreen* CustomComponent::getResolvedScreen() const
{
    return resolvedScreen.has_value() ? &resolvedScreen.value() : nullptr;
}

void CustomComponent::registerInternals(Simulator* sim,
                                       const std::unordered_map<std::string, CustomComponentDef>& customDefs)
{
    if (!internalComps.empty())
        unregisterInternals(sim);

    setSimulator(sim);
    resolvedScreen.reset();

    // Create a temporary canvas attached to this simulator.
    // Nested custom components (e.g. 1-Bit Switch inside 4-Bit Switch) need the
    // parent's customDefs so makeComponent can resolve them during deserialize.
    Canvas tempCanvas(sim);
    tempCanvas.customDefs = customDefs;
    tempCanvas.deserialize(def.canvasJson, Canvas::DeserializeMode::RestoreInternalOnly);

    // Steal all the instantiated components
    std::unordered_map<int, Component*> compMap;
    for (auto& cv : tempCanvas.comps) {
        if (!cv.comp) continue;
        compMap[cv.id] = cv.comp.get();
        internalComps.push_back(std::move(cv.comp));
    }
    
    // Gather nets that were created
    // Note: tempCanvas wires hold raw Net pointers. Since Simulator owns them, we just extract them.
    // Wait, Simulator owns them? 
    // In Simulator.hpp: Net* createNet(); We MUST tell Simulator to delete them.
    // We will keep them in a vector of Net* to delete later.
    for (auto& wv : tempCanvas.wires) {
        if (wv.net && std::find(rawInternalNets.begin(), rawInternalNets.end(), wv.net) == rawInternalNets.end()) {
            rawInternalNets.push_back(wv.net);
        }
        for (auto* n : wv.busNets) {
            if (n && std::find(rawInternalNets.begin(), rawInternalNets.end(), n) == rawInternalNets.end()) {
                rawInternalNets.push_back(n);
            }
        }
    }

    if (def.screen.has_value()) {
        ResolvedScreen rs;
        rs.layout = def.screen.value();
        for (const auto& pd : def.screen->pixels) {
            auto it = compMap.find(pd.internalCompId);
            if (it == compMap.end()) continue;
            auto* rgb = dynamic_cast<RGBDisplay*>(it->second);
            if (!rgb) continue;
            rs.pixels.push_back({rgb, pd.col, pd.row});
        }
        if (!rs.pixels.empty())
            resolvedScreen = std::move(rs);
    }

    // Now map PORT_IN and PORT_OUT to our external pins
    this->receivers.clear();
    this->drivers.clear();

    for (const auto& pDef : def.inPorts) {
        if (auto* c = compMap[pDef.internalCompId]) {
            for (int i=0; i<c->numReceivers(); ++i) {
                this->receivers.push_back(c->getReceiver(i));
            }
        }
    }
    for (const auto& pDef : def.outPorts) {
        if (auto* c = compMap[pDef.internalCompId]) {
            for (int i=0; i<c->numDrivers(); ++i) {
                this->drivers.push_back(c->getDriver(i));
            }
        }
    }
}

void CustomComponent::unregisterInternals(Simulator* sim)
{
    for (auto& comp : internalComps) {
        if (auto* c = dynamic_cast<Clock*>(comp.get())) {
            sim->unregisterClock(c);
        } else {
            sim->unregisterComponent(comp.get());
        }
    }
    for (Net* n : rawInternalNets) {
        sim->removeNet(n);
    }
    rawInternalNets.clear();
    internalComps.clear();
    resolvedScreen.reset();
}
