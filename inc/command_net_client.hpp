#ifndef COMMAND_NET_CLIENT_HPP
#define COMMAND_NET_CLIENT_HPP

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

#endif // COMMAND_NET_CLIENT_HPP
