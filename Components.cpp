#include "Components.hpp"
#include "Pin.hpp"

Component::~Component() {
    for (Pin* pin : inputs) {
        delete pin;
    }

    for (Pin* pin : outputs) {
        delete pin;
    }
}