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

    ~WatchdogTimer() {
        stop();
        if (watchdogThread.joinable()) {
            watchdogThread.join();
        }
    }

    void start() {
        if (onTimeoutCallback && timeout != std::chrono::duration<double>::zero()) {
            watchdogThread = std::thread(&WatchdogTimer::watchdogLoop, this);
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
            if (cv.wait_for(lock, timeout, [this] { return resetFlag || stopFlag; })) {
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

    std::chrono::duration<double> timeout;
    std::atomic<bool> stopFlag;
    bool resetFlag;
    bool pauseFlag;
    std::mutex mtx;
    std::condition_variable cv;
    std::thread watchdogThread;

public:
    std::function<void()> onTimeoutCallback;

};