#include <spdlog/spdlog.h>

#include "net_client.hpp"

NetClient::NetClient() {}

void NetClient::connect(std::string host, int port) {
    // setup variables
    host_ = host;
    port_ = port;

    struct sockaddr_in server_addr;

    // use DNS to get IP address
    struct hostent *hostEntry;
    hostEntry = gethostbyname(host_.c_str());
    if (!hostEntry) {
        SPDLOG_ERROR("No such host name: {}", host_);
        return;
    }

    // setup socket address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    memcpy(&server_addr.sin_addr, hostEntry->h_addr_list[0], hostEntry->h_length);

    // create socket
    socket_ = socket(PF_INET, SOCK_STREAM, 0);
    if (!socket_) {
        SPDLOG_ERROR("Socket creation");
        return;
    }

    // Set the receive timeout  DOES NOT WORK
    // struct timeval tv;
    // tv.tv_sec = 1;  // Timeout in seconds
    // tv.tv_usec = 0; // 0 microseconds

    // if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
    //     SPDLOG_ERROR("setsockopt - SO_RCVTIMEO");
    //     close(socket_);
    //     return;
    // }

    // // Set the send timeout
    // if (setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
    //     SPDLOG_ERROR("setsockopt - SO_SNDTIMEO");
    //     close(socket_);
    //     return;
    // }

    // connect to server
    if (::connect(socket_, reinterpret_cast<const struct sockaddr *>(&server_addr), sizeof(server_addr)) < 0) {
        SPDLOG_ERROR("Connect");
        return;
    } else {
        SPDLOG_INFO("Connected to PORT: {}", port_);
        is_connected = true;
    }
}

void NetClient::reconnect() {
    close();
    connect(host_, port_);
}

void NetClient::close() {
    ::shutdown(socket_, SHUT_RDWR);     // It will make a recv() call to finish waiting for data
    ::close(socket_);
}

bool NetClient::send_request(std::string request) {
    try {
        // prepare to send request
        const char *ptr = request.c_str();
        int nleft = request.length();
        int nwritten;
        // loop to be sure it is all sent
        while (nleft) {
            if ((nwritten = ::send(socket_, ptr, nleft, MSG_NOSIGNAL)) < 0) {
                if (errno == EINTR) {
                    // the socket call was interrupted -- try again
                    is_connected = false;
                    continue;
                } else {
                    // an error occurred, so break out
                    SPDLOG_ERROR("Write");
                    is_connected = false;
                    return false;
                }
            } else if (nwritten == 0) {
                // the socket is closed
                is_connected = false;
                return false;
            }
            nleft -= nwritten;
            ptr += nwritten;
        }
        return true;
    } catch (std::exception &e) {
        is_connected = false;
        SPDLOG_ERROR("Error sending {}", e.what());
        return false;
    }
}

std::string NetClient::get_response() {
    int nread = ::recv(socket_, buf_, buflen_, 0);
    if (nread < 0) {
        if (errno == EINTR) {
            // the socket call was interrupted -- try again
            return "";
        } else {
            // an error occurred, so break out
            return "";
        }
    } else if (nread == 0) {
        // the socket is closed
        return "";
    }
    // be sure to use append in case we have binary data
    return std::string(buf_);
}
