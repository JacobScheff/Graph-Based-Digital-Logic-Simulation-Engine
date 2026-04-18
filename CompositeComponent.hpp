#include "Components.hpp"
#include "Net.hpp"
#include "Pin.hpp"
#include <vector>
#include <map>

class CompositeComponent : public Component
{
public:
    ~CompositeComponent() {
        // TODO
    };

    void update(Receiver* updatedReciever) override {
        // Check for external input changes
        for (Port& port : inputPorts) {
            if (port.rx == updatedReciever) {
                port.tx->setState(updatedReciever->getState());
                break;
            }
        }

        // Check for internal output changes
        for (Port& port : outputPorts) {
            if (port.rx == updatedReciever) {
                port.tx->setState(updatedReciever->getState());
                break;
            }
        }
    };
    
protected:
    struct Port {
        Receiver* rx;
        Driver* tx;
    };

    std::vector<Port> inputPorts;
    std::vector<Port> outputPorts;

    std::vector<Component*> internalComponents;
    std::vector<Net*> internalNets;
};