#ifndef TOUCH_PROBE_HPP
#define TOUCH_PROBE_HPP

#include <iostream>



enum class State {
    Idle,
    InitialTouch,
    DebouncingTouch,
    TouchDetected,
    NewCommand,
    InitialRelease,
    DebouncingRelease,
};



class TouchProbeFSM {
public:
    TouchProbeFSM() noexcept: currentState(State::Idle), debounceCount(0) {}

    void process(bool touch_probe) {
        switch (currentState) {
            case State::Idle:
                if (!touch_probe) {
                    currentState = State::InitialTouch;
                }
                break;

            case State::InitialTouch:
                if (touch_probe) {
                    currentState = State::Idle;  // Noise or release event
                } else {
                    debounceCount = 0;
                    currentState = State::DebouncingTouch;
                }
                break;

            case State::DebouncingTouch:
                if (debounceCount >= DEBOUNCE_THRESHOLD) {
                    currentState = State::TouchDetected;
                } else if (touch_probe) {
                    currentState = State::Idle;  // Noise or release event during debounce
                } else {
                    debounceCount++;
                }
                break;

            case State::TouchDetected:
                if (touch_probe) {
                    currentState = State::InitialRelease;
                }
                break;

            case State::NewCommand:
                if (!touch_probe) {
                    currentState = State::InitialRelease;
                }
                break;

            case State::InitialRelease:
                if (!touch_probe) {
                    currentState = State::Idle;  // New movement requested
                } else {
                    debounceCount = 0;
                    currentState = State::DebouncingRelease;
                }
                break;

            case State::DebouncingRelease:
                if (touch_probe) {
                    currentState = State::Idle;  // Noise or release event during debounce
                } else {
                    debounceCount++;
                }
                break;
        }
    }

    void newCommand() {
        currentState = State::NewCommand;
    }

    bool isTouchDetected() const {
        return currentState == State::TouchDetected;
    }

private:
    State currentState;
    int debounceCount;
    static const int DEBOUNCE_THRESHOLD = 3; // Adjust this value for your needs
};


#endif      // TOUCH_PROBE_HPP
