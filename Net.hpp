#include <vector>
#include "Pin.hpp"

enum class State {
    LOW = 0,
    HIGH = 1,
    FLOATING = 2,
    UNDEFINED = 3
};

class Net {
    public:
        void addDriver(Pin* pin) {
            drivers.push_back(pin);
            driverStateCounts[static_cast<int>(pin->getState())]++;
            updateState();
        }

        void addReceiver(Pin* pin) {
            receivers.push_back(pin);
        }

        void broadcastStateToReceivers() {
            TODO!
        }

        void update(State oldState, State newState) {
            if (oldState != newState) {
                driverStateCounts[static_cast<int>(oldState)]--;
                driverStateCounts[static_cast<int>(newState)]++;
                updateState();
            }
        }

        void updateState() {
            // FLOATING pins do not affect the state, so start with FLOATING as the default state
            state = State::FLOATING;

            // If there are already UNDEFINED drivers, the state is UNDEFINED
            if (driverStateCounts[static_cast<int>(State::UNDEFINED)] > 0) {
                state = State::UNDEFINED;
                return;
            }

            // Check LOW and HIGH states
            if (driverStateCounts[static_cast<int>(State::LOW)] > 0) {
                state = State::LOW;
            }
            if (driverStateCounts[static_cast<int>(State::HIGH)] > 0) {
                if (state == State::LOW) {
                    state = State::UNDEFINED; // Conflict between LOW and HIGH drivers results in UNDEFINED state
                } else {
                    state = State::HIGH;
                }
            }
        }

        // void updateState() {
        //     state = State::FLOATING;

        //     // If there are already undefined drivers, the state is undefined
        //     if (driverStateCounts[static_cast<int>(State::UNDEFINED)] > 0) {
        //         state = State::UNDEFINED;
        //         return;
        //     }

        //     // Only need to check for LOW and HIGH states. If floating, there is no contribution to the state. If there is undefined, it has already been handled.
        //     for (int i = 0; i < 2; i++) {
        //         if (driverStateCounts[i] > 0) {
        //             state = resolveState(state, static_cast<State>(i));

        //             // If the state is undefined, no need to check further
        //             if (state == State::UNDEFINED) break;
        //         }
        //     }
        // }

        // State resolveState(State state1, State state2) const {
        //     // If any is undefined, the overall state is undefined
        //     if (state1 == State::UNDEFINED || state2 == State::UNDEFINED) return State::UNDEFINED;

        //     // If one is floating, the overall state is the other
        //     if (state1 == State::FLOATING) return state2;
        //     if (state2 == State::FLOATING) return state1;

        //     // Otherwise, both are either HIGH or LOW, so return that state if they match
        //     if (state1 == state2) return state1;

        //     // If they differ, the overall state is undefined
        //     return State::UNDEFINED;
        // }

    private:
        std::vector<Pin*> drivers;
        int driverStateCounts[4] = {0, 0, 0, 0}; // LOW, HIGH, FLOATING, UNDEFINED
        std::vector<Pin*> receivers;
        State state = State::UNDEFINED;
};