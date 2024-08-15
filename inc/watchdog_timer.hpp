#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <thread>

class WatchdogTimer {
  public:  
    WatchdogTimer() = default;

    WatchdogTimer(std::chrono::duration<double> timeout_, std::function<void()> timeoutCallback)
        : timeout(timeout_), onTimeoutCallback(timeoutCallback){
            start();
    }

    void start() {
        if (onTimeoutCallback && timeout != std::chrono::duration<double>::zero()) {
            watchdogThread = std::jthread(&WatchdogTimer::watchdogLoop, this);
        }
    }

    void start(std::chrono::duration<double> timeout_) {
        timeout = timeout_;
        start();
    }

    void reset() {
        std::unique_lock<std::mutex> lock(mtx);
        resetFlag = true;
        cv.notify_one();
    }

    void pause() {
        std::unique_lock<std::mutex> lock(mtx);
        pauseFlag = true;
        cv.notify_one();
    }

    void resume() {
        std::unique_lock<std::mutex> lock(mtx);
        resetFlag = true;
        pauseFlag = false;
        cv.notify_one();
    }

  private:
    void watchdogLoop(std::stop_token stop_token) {
        while (!stop_token.stop_requested()) {
            std::unique_lock<std::mutex> lock(mtx);
            if (cv.wait_for(lock, timeout, [this] { return resetFlag; })) {
                // Timer was reset
                resetFlag = false;
            } else {
                if (pauseFlag) {
                    continue;
                }
                // Timeout expired without reset
                pauseFlag = true; // Once it expired stay paused, until resumed
                onTimeoutCallback();
            }
        }
    }

    std::chrono::duration<double> timeout;
    bool resetFlag;
    bool pauseFlag;
    std::mutex mtx;
    std::condition_variable cv;
    std::jthread watchdogThread;

public:
    std::function<void()> onTimeoutCallback;

};