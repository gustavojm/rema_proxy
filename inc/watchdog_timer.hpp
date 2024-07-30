#pragma once

#include <iostream>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <functional>


class WatchdogTimer {
public:
    WatchdogTimer(int seconds, std::function<void()> timeoutCallback): 
        timeoutSeconds(seconds), 
        onTimeoutCallback(timeoutCallback)
         {
        watchdogThread = std::thread(&WatchdogTimer::watchdogLoop, this);
    }

    ~WatchdogTimer() {
        stop();
        if (watchdogThread.joinable()) {
            watchdogThread.join();
        }
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

    void stop() {
        {
            std::unique_lock<std::mutex> lock(mtx);
            stopFlag = true;
            cv.notify_one();
        }
        if (watchdogThread.joinable()) {
            watchdogThread.join();
        }
    }

private:
    void watchdogLoop() {
        std::unique_lock<std::mutex> lock(mtx);
        while (!stopFlag) {
            if (cv.wait_for(lock, std::chrono::seconds(timeoutSeconds), [this] { return resetFlag || stopFlag; })) {
                // Timer was reset or stopped
                if (stopFlag) {
                    break;
                }
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

    int timeoutSeconds;
    std::function<void()> onTimeoutCallback;
    std::atomic<bool> stopFlag;
    bool resetFlag;
    bool pauseFlag;
    std::mutex mtx;
    std::condition_variable cv;
    std::thread watchdogThread;
};