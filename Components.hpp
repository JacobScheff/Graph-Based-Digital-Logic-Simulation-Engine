#pragma once
#include <vector>
#include <map>

class Pin;
class Receiver;
class Driver;

class Component
{
public:
    virtual ~Component();

    // Called by the component to update the state of its drivers after processing changes in its receivers
    virtual void update(Receiver* updatedReceiver) = 0;

    Receiver* getInput(int index) {
        return inputs[index];
    }

    Driver* getOutput(int index) {
        return outputs[index];
    }

protected:    
    std::vector<Receiver*> inputs;
    std::vector<Driver*> outputs;
};