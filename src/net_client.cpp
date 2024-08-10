#include <spdlog/spdlog.h>

#include "net_client.hpp"
#include <fcntl.h>

NetClient::NetClient() {
}

int NetClient::connect(std::string host, int port, int nsec) {
    // setup variables
    host_ = host;
    port_ = port;

    struct sockaddr_in server_addr;

    // use DNS to get IP address
    struct hostent *hostEntry;
    hostEntry = gethostbyname(host_.c_str());
    if (!hostEntry) {
        SPDLOG_ERROR("No such host name: {}", host_);
        return -1;
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
        return -1;
    }

    // connect to server
    int flags, n, error;
    socklen_t len;
    fd_set rset, wset;
    struct timeval tval;

    flags = ::fcntl(socket_, F_GETFL, 0);
    ::fcntl(socket_, F_SETFL, flags | O_NONBLOCK);

    error = 0;
    if ((n = ::connect(socket_, reinterpret_cast<const struct sockaddr *>(&server_addr), sizeof(server_addr))) < 0) {
        if (errno != EINPROGRESS) {
            return (-errno);
        }
    }

    /* Do whatever we want while the connect is taking place. */
    SPDLOG_INFO("Conecting to {}:{}", host, port);

    if (n == 0) {
        goto done; /* connect completed immediately */
    }

    FD_ZERO(&rset);
    FD_SET(socket_, &rset);
    wset = rset;
    tval.tv_sec = nsec;
    tval.tv_usec = 0;

    if ((::select(socket_ + 1, &rset, &wset, nullptr, nsec ? &tval : nullptr)) == 0) {
        ::close(socket_); /* timeout */
        errno = ETIMEDOUT;
        return (-errno);
    }

    if (FD_ISSET(socket_, &rset) || FD_ISSET(socket_, &wset)) {
        len = sizeof(error);
        if (::getsockopt(socket_, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
            return (-error); /* Solaris pending error */
        }
    } else {
        SPDLOG_ERROR("Select error: sockfd not set");
    }

done:
    ::fcntl(socket_, F_SETFL, flags); /* restore file status flags */

    if (error) {
        ::close(socket_); /* just in case */
        errno = error;
        return (-errno);
    }
    SPDLOG_INFO("Connected to PORT: {}", port_);
    is_connected = true;
    return (0);
}

void NetClient::reconnect() {
    close();
    connect(host_, port_);
}

int getSO_ERROR(int fd) {
    int err = 1;
    socklen_t len = sizeof err;
    if (-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&err), &len))
        SPDLOG_ERROR("getSO_ERROR");
    if (err) {
        errno = err; // set errno to the socket SO_ERROR
    }
    return err;
};

void shutdownSocket(int fd) {
    if (fd >= 0) {
        getSO_ERROR(fd);                                // first clear any errors, which can cause close to fail
        if (::shutdown(fd, SHUT_RDWR) < 0) {            // secondly, terminate the 'reliable' delivery
            if (errno != ENOTCONN && errno != EINVAL) { // SGI causes EINVAL
                SPDLOG_ERROR("shutdown");
            }
        }
    }
};

void NetClient::close() {
    is_connected = false;
    shutdownSocket(socket_); // As I'm shutind down socket_ from another thread, use shutdown instead of close
                             // otherwise the same socket number will be assigned and errno = EBADF
}

bool NetClient::send_request(std::string request) {
    if (is_connected) {
        try {
            // prepare to send request
            const char *ptr = request.c_str();
            size_t nleft = request.length();
            ssize_t nwritten = 0;
            // loop to be sure it is all sent
            while (nleft) {
                if (nwritten = ::send(socket_, ptr, nleft, MSG_NOSIGNAL); nwritten < 0) {
                    if (errno == EINTR) {
                        // the socket call was interrupted -- try again
                        continue;
                    } else {
                        // an error occurred, so break out
                        SPDLOG_ERROR("Error writing to socket");
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
    return false;
}

std::string NetClient::get_response() {
    if (is_connected) {
        if (!leftover_buffer.empty()) {            
            if (std::size_t null_pos; (null_pos = leftover_buffer.find('\0')) != std::string::npos) {
                std:: string prev = leftover_buffer.substr(0, null_pos - 1);
                leftover_buffer = leftover_buffer.substr(null_pos + 1);
                return prev;
            }
        }

                                                            // If something was left on leftover_buffer because no null was found...
        std::string response = std::move(leftover_buffer);  // start with what was left

        // Read until we get a null character
        while (true) {
            ssize_t nread = recv(socket_, buf_, buflen_, 0);
            if (nread < 0) {
                if (errno == EINTR) {
                    // The socket call was interrupted -- try again
                    continue;
                } else {
                    // An error occurred, so return false
                    return {};
                }
            } else if (nread == 0) {
                // The socket is closed
                return {};
            }
            
            for (ssize_t i = 0; i < nread; ++i) {
                if (buf_[i] == '\0') {
                    // Guardar lo que viene después del carácter nulo en el buffer restante
                    leftover_buffer.assign(buf_ + i + 1, nread - i - 1);
                    return response; // Retornar la respuesta acumulada hasta el carácter nulo
                }
                response += buf_[i];
            }
        }
    }
    return {};
}

std::vector<uint8_t> NetClient::get_response_binary() {
    if (is_connected) {
        std::vector<uint8_t> response;

        while (true) {
            ssize_t nread = recv(socket_, buf_, buflen_, 0);
            if (nread < 0) {
                if (errno == EINTR) {
                    // The socket call was interrupted -- try again
                    continue;
                } else {
                    // An error occurred, so return false
                    return {};
                }
            } else if (nread == 0) {
                // The socket is closed
                return {};
            }

            for (ssize_t i = 0; i < nread; ++i) {
                response.push_back(buf_[i]);
            }
            return response;
        }
    }
    return {};
}
