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

class TelemetryNetClient : public NetClient {
  public:
    TelemetryNetClient(std::function<void(std::string)> onReceiveCallback) : NetClient(), lambda(onReceiveCallback)  {
        thd = std::unique_ptr<std::thread>(new std::thread([=] { this->loop(); }));
    }

    ~TelemetryNetClient() {
        done = true;
        thd->join();
    }

    void loop() {
        while (!done) {
            std::string line = get_response();
            if (!line.empty()) {
                // std::cout << "t" << std::flush;
                lambda(line);
            }
        }
    }

    std::function<void(std::string)> lambda;
    bool done = false;
    std::unique_ptr<std::thread> thd;
};

#endif // TELEMETRY_NET_CLIENT_HPP
