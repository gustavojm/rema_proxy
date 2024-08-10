#pragma once

#include "net_client.hpp"
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <watchdog_timer.hpp>

class LogsNetClient : public NetClient {
  public:
    LogsNetClient() = default;

    void set_on_receive_callback(std::function<void(std::string&)> onReceiveCallback) {
        onReceiveCb = onReceiveCallback;
    }

    ~LogsNetClient() {
        if (thd.joinable()) {
            stop();
        }
    }

    void start() {
        if (!alreadyStarted) {
            thd = std::thread(&LogsNetClient::loop, this);
            alreadyStarted = true;
        }
    }

    void stop() {
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

            if (is_connected) {
                std::string line = get_response();
                if (!line.empty()) {
                    onReceiveCb(line);
                }
            }
        }
    }

    std::function<void(std::string&)> onReceiveCb;
    std::thread thd;
    std::mutex mtx;
    std::condition_variable cv;
    bool suspendFlag = false;
    bool stopFlag = false;
    bool alreadyStarted = false;
    int ConnectionTimeout = 5;
};
