#pragma once

#include "net_client.hpp"
#include <chrono>
#include <iostream>
#include <restbed>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>

class CommandNetClient : public NetClient {
  public:
    CommandNetClient() {
    }

    ~CommandNetClient() {
    }
};

