#pragma once

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <string>

class Client {
public:
    Client(std::string host, int port);
    virtual ~Client() = default;

    virtual void create();
    virtual void close_socket();
    bool send_request(std::string);
    std::string get_response();
    int get_port() {        
        return port_;
    }
    
    std::string get_host() {        
        return host_;
    }
    
    bool is_connected = false;

private:
    std::string host_;
    int port_;
    int socket_;
    int buflen_;
    char* buf_;
    
};
