#ifndef COMMAND_NET_CLIENT_HPP
#define COMMAND_NET_CLIENT_HPP

#include <chrono>
#include <restbed>
#include <spdlog/spdlog.h>
#include <string>
#include <iostream>
#include <thread>
#include <memory>
#include "client.hpp"


class CommandNetClient : public Client {
  public:

    CommandNetClient(std::string host, int port) : Client(host, port) {
        //thd = std::unique_ptr<std::thread>(new std::thread([=] { this->loop(); }));
    }

    ~CommandNetClient() {
      //done = true;
      //thd->join();
    }

    std::function<void(std::string)> lambda;
    //bool done = false;
    //std::unique_ptr<std::thread> thd;
};

#endif // COMMAND_NET_CLIENT_HPP
