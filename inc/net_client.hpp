#pragma once

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <string>

class NetClient {
  public:
    NetClient();
    virtual ~NetClient() = default;

    virtual int connect(std::string host, int port, int nsec = 5);

    virtual void close();

    virtual void reconnect();
    
    bool send_request(std::string);
    
    std::string get_response();
    
    int get_port() {
        return port_;
    }

    std::string get_host() {
        return host_;
    }

    volatile bool is_connected = false;

  private:
    std::string host_;
    int port_;
    volatile int socket_;
    int buflen_ = 1024;
    char *buf_ = new char[buflen_];
};
