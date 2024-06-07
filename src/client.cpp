#include "client.hpp"

Client::Client(std::string host, int port) {
    // setup variables
    host_ = host;
    port_ = port;
    buflen_ = 1024;
    buf_ = new char[buflen_+1];

    create();
}

void Client::create() {
    struct sockaddr_in server_addr;

    // use DNS to get IP address
    struct hostent *hostEntry;
    hostEntry = gethostbyname(host_.c_str());
    if (!hostEntry) {
        std::cout << "No such host name: " << host_ << std::endl;
        exit(-1);
    }

    // setup socket address structure
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    memcpy(&server_addr.sin_addr, hostEntry->h_addr_list[0], hostEntry->h_length);

    // create socket
    socket_ = socket(PF_INET,SOCK_STREAM,0);
    if (!socket_) {
        perror("socket");
        exit(-1);
    }

    // Set the receive timeout
    struct timeval tv;
    tv.tv_sec = 1;  // Timeout in seconds
    tv.tv_usec = 0; // 0 microseconds

    if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
        perror("setsockopt - SO_RCVTIMEO");
        close(socket_);
        return;
    }

    // Set the send timeout
    if (setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
        perror("setsockopt - SO_SNDTIMEO");
        close(socket_);
        return;
    }

    // connect to server
    if (connect(socket_,(const struct sockaddr *)&server_addr,sizeof(server_addr)) < 0) {
        perror("connect");
        exit(-1);
    } else {
        std::cout << "Connected to PORT:" << port_ << "\n";
        is_connected = true;
    }

    const int timeout = 2;
    ::setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof timeout);//SO_SNDTIMEO for send ops
    ::setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof timeout);//SO_SNDTIMEO for send ops
}

void Client::close_socket() {
    close(socket_);
}

bool Client::send_request(std::string request) {
    try {
    // prepare to send request
    const char* ptr = request.c_str();
    int nleft = request.length();
    int nwritten;
    // loop to be sure it is all sent
    while (nleft) {
        if ((nwritten = send(socket_, ptr, nleft, MSG_NOSIGNAL)) < 0) {
            if (errno == EINTR) {
                // the socket call was interrupted -- try again
                is_connected = false;
                continue;
            } else {
                // an error occurred, so break out
                perror("write");
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
    } catch(std::exception &e) {
        is_connected = false;
        std::cout << e.what();        
        return false;
    }
}

std::string Client::get_response() {
    int nread = recv(socket_,buf_,1024,0);
    if (nread < 0) {
        if (errno == EINTR){ 
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
