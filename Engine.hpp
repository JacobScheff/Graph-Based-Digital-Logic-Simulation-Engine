#pragma once
#include <vector>
#include <map>
#include <stdexcept>

#include "Components.hpp"
#include "Net.hpp"
#include "Pin.hpp"

class Engine
{
public:
    struct Update {
        
    }

    ~Engine()
    {
        for (Component *component : components)
        {
            delete component;
        }

        for (Net *net : nets)
        {
            delete net;
        }
    };

    void addComponent(Component *component)
    {
        components.push_back(component);
    }

    void addNet(Net *net)
    {
        nets.push_back(net);
    }

    void addCircuitInput(Driver* driver) {
        circuitInputs.push_back(driver);
    }

    void addCircuitOutput(Receiver* receiver) {
        circuitOutputs.push_back(receiver);
    }

    // Process all updates
    void tick() {

    }

private:
    std::vector<Component *> components;
    std::vector<Net *> nets;
    std::vector<Driver* > circuitInputs;
    std::vector<Receiver* > circuitOutputs;
}