#include "Net.hpp"
#include <vector>
#include <map>

class Component
{
public:
    // Called by the component to update the state of its drivers after processing changes in its receivers
    virtual void update(Receiver* updatedReceiver) {
        
    }

private:    
    std::vector<Component*> components;
};

// TODO: Implement gates
class Gate : public Component
{
    
};