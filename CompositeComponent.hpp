#pragma once
#include <vector>
#include <map>
#include <stdexcept>
#include <initializer_list>

#include "Components.hpp"
#include "Net.hpp"
#include "Pin.hpp"

class CompositeComponent : public Component
{
public:
    CompositeComponent(int inputSize, int outputSize) : inputSize(inputSize), outputSize(outputSize) {
        for (int i = 0; i < inputSize; i++) {
            inputPorts.push_back({nullptr, nullptr});
        }

        for (int i = 0; i < outputSize; i++) {
            outputPorts.push_back({nullptr, nullptr});
        }
    };

    ~CompositeComponent() {
        for (Component* component : internalComponents) {
            delete component;
        }

        for (Net* net : internalNets) {
            delete net;
        }
    };

    void update(Receiver* updatedReciever) override {
        // Check for external input changes
        std::map<Receiver*, int>::iterator externalIt = recieverToInputPinIndex.find(updatedReciever);
        if (externalIt != recieverToInputPinIndex.end()) {
            Port inputPort = inputPorts[externalIt->second];
            inputPort.tx->setState(updatedReciever->getState());
        }

        // Check for internal output changes
        std::map<Receiver*, int>::iterator internalIt = receiverToOutputPinIndex.find(updatedReciever);
        if (internalIt != receiverToOutputPinIndex.end()) {
            Port outputPort = outputPorts[internalIt->second];
            outputPort.tx->setState(updatedReciever->getState());
        }
    };

    void addInputPort(Receiver* rx, Driver* tx, int portIndex) {
        if (portIndex < 0 || portIndex > inputSize - 1) {
            throw std::out_of_range("Input port index out of range");
        }

        if (inputPorts[portIndex].rx != nullptr) {
            throw std::runtime_error("Input port already occupied");
        }

        inputPorts[portIndex].rx = rx;
        inputPorts[portIndex].tx = tx;
        recieverToInputPinIndex[rx] = portIndex;
    };

    void addOutputPort(Receiver* rx, Driver* tx, int portIndex) {
        if (portIndex < 0 || portIndex > outputSize - 1) {
            throw std::out_of_range("Output port index out of range");
        }

        if (outputPorts[portIndex].tx != nullptr) {
            throw std::runtime_error("Output port already occupied");
        }

        outputPorts[portIndex].rx = rx;
        outputPorts[portIndex].tx = tx;
        receiverToOutputPinIndex[rx] = portIndex;
    };

    CompositeComponent& add(Component* component) {
        internalComponents.push_back(component);
        return *this;
    };

    CompositeComponent& add(Net* net) {
        internalNets.push_back(net);
        return *this;
    };

    void addInternalComponent(Component* component) {
        add(component);
    };

    void addInternalNet(Net* net) {
        add(net);
    };

private:
    int inputSize;
    int outputSize;

    struct Port {
        Receiver* rx;
        Driver* tx;
    };

    std::vector<Port> inputPorts;
    std::vector<Port> outputPorts;

    std::map<Receiver*, int> recieverToInputPinIndex;
    std::map<Receiver*, int> receiverToOutputPinIndex;

    std::vector<Component*> internalComponents;
    std::vector<Net*> internalNets;
};