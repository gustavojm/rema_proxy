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
#include <watchdog_timer.hpp>

class TelemetryNetClient : public NetClient {
  public:
    TelemetryNetClient(std::function<void(std::string)> onReceiveCallback) : 
    NetClient(), 
    onReceiveCb(onReceiveCallback),
    wd(LostConnectionTimeout, [&]{close();})
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
        wd.resume();
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
                    //std::cout << "t" << std::flush;
                    onReceiveCb(line);
                    wd.reset();
                }
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
    int LostConnectionTimeout = 2;
    int ConnectionTimeout = 5;
    WatchdogTimer wd;

};

#endif // TELEMETRY_NET_CLIENT_HPP
