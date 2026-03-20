#include "Gate.cpp"

class And : public Gate
{
public:
    bool evaluate(bool input1, bool input2) override
    {
        return input1 && input2;
    }
};