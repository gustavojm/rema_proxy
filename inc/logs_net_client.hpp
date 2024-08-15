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

    void start() {
        if (!alreadyStarted) {
            thd = std::jthread(&LogsNetClient::loop, this);
            alreadyStarted = true;
        }
    }

    int connect(std::string host, int port, int nsec = 0) override {
        nsec = (nsec == 0 ? ConnectionTimeout : nsec);
        if (int n; (n = NetClient::connect(host, port, nsec)) < 0) {
            return n;
        }
        return 0;
    }

    void loop(std::stop_token stop_token) {
        while (!stop_token.stop_requested()) {
            if (is_connected) {
                std::string line = get_response();
                if (!line.empty()) {
                    onReceiveCb(line);
                }
            }
        }
    }

    std::function<void(std::string&)> onReceiveCb;
    std::jthread thd;
    bool alreadyStarted = false;
    int ConnectionTimeout = 5;
};
