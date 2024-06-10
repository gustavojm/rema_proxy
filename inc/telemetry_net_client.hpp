#ifndef TELEMETRY_NET_CLIENT_HPP
#define TELEMETRY_NET_CLIENT_HPP

#include "net_client.hpp"
#include <chrono>
#include <iostream>
#include <memory>
#include <restbed>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <condition_variable>

class TelemetryNetClient : public NetClient {
  public:
    TelemetryNetClient(std::function<void(std::string)> onReceiveCallback) : 
    NetClient(), 
    onReceiveCb(onReceiveCallback)
    {}

    ~TelemetryNetClient() {
        if (thd.joinable()) {
            stop();
        }
    }

    void start() {
        if (!alreadyStarted) {
            thd = std::thread(&TelemetryNetClient::loop, this);
            alreadyStarted = true;
        }
    }

    void suspend() {
        std::unique_lock<std::mutex> lock(mtx);
        suspendFlag = true;
    }

    void resume() {
        {
            std::unique_lock<std::mutex> lock(mtx);
            suspendFlag = false;
        }
        cv.notify_one();
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


    void loop() {
        while (true) {
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [this] { return !suspendFlag || stopFlag; });

                if (stopFlag) {
                    break;
                }
            }

            std::string line = get_response();
            if (!line.empty()) {
                //std::cout << "t" << std::flush;
                onReceiveCb(line);
            }
        }
    }

    std::function<void(std::string)> onReceiveCb;
    std::thread thd;
    std::mutex mtx;
    std::condition_variable cv;
    bool suspendFlag = false;
    bool stopFlag = false;
    bool alreadyStarted = false;

};

#endif // TELEMETRY_NET_CLIENT_HPP
