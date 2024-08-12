#pragma once

#include "net_client.hpp"
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <restbed>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <watchdog_timer.hpp>

class TelemetryNetClient : public NetClient {
  public:
    TelemetryNetClient() = default;

    ~TelemetryNetClient() {
        if (alreadyStarted && thd.joinable()) {
            stop();
        }
    }

    void set_on_receive_callback(std::function<void(std::vector<uint8_t>&)> onReceiveCallback) {
        onReceiveCb = onReceiveCallback;
    }

    void start() {
        if (!alreadyStarted && onReceiveCb) {
            thd = std::thread(&TelemetryNetClient::loop, this);
            disconnect_watchdog.onTimeoutCallback = [&] { std::cout << "CLOSING TIMEOUTTTTTT" << std::endl; close(); };
            disconnect_watchdog.start(std::chrono::seconds(2));
            alreadyStarted = true;
        }
    }

    void stop() {
        disconnect_watchdog.stop();
        {
            std::unique_lock<std::mutex> lock(mtx);
            stopFlag = true;
            suspendFlag = false; // Ensure it doesn't stay suspended when stopping
        }
        cv.notify_one();
        thd.join();
    }

    int connect(std::string host, int port, int nsec = 0) override {
        nsec = (nsec == 0 ? ConnectionTimeout : nsec);
        if (int n; (n = NetClient::connect(host, port, nsec)) < 0) {
            return n;
        }
        disconnect_watchdog.resume();
        return 0;
    }

    void loop() {
        while (true) {
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [this] { return !suspendFlag || stopFlag; });

                if (stopFlag) {
                    break;
                }
            }

            std::vector<uint8_t> line = get_response_binary();
            if (!line.empty()) {
                // std::cout << "t" << std::flush;
                onReceiveCb(line);
                disconnect_watchdog.reset();
            }
        }
    }

    std::function<void(std::vector<uint8_t>&)> onReceiveCb;
    std::thread thd;
    std::mutex mtx;
    std::condition_variable cv;
    bool suspendFlag = false;
    bool stopFlag = false;
    bool alreadyStarted = false;    
    int ConnectionTimeout = 5;
    WatchdogTimer disconnect_watchdog;
};
