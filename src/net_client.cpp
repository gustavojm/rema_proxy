#include <spdlog/spdlog.h>

#include "net_client.hpp"
#include <fcntl.h>

NetClient::NetClient() {}

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

	int				flags, n, error;
	socklen_t		len;
	fd_set			rset, wset;
	struct timeval	tval;

	flags = ::fcntl(socket_, F_GETFL, 0);
	::fcntl(socket_, F_SETFL, flags | O_NONBLOCK);

	error = 0;
	if ((n = ::connect(socket_, reinterpret_cast<const struct sockaddr *>(&server_addr), sizeof(server_addr))) < 0)
		if (errno != EINPROGRESS)
			return(-1);

	/* Do whatever we want while the connect is taking place. */
    SPDLOG_INFO("Conecting to {}:{}", host, port);

	if (n == 0)
		goto done;	/* connect completed immediately */

	FD_ZERO(&rset);
	FD_SET(socket_, &rset);
	wset = rset;
	tval.tv_sec = nsec;
	tval.tv_usec = 0;

	if ( (n = ::select(socket_+1, &rset, &wset, NULL,
					 nsec ? &tval : NULL)) == 0) {
		::close(socket_);		/* timeout */
		errno = ETIMEDOUT;
		return(-1);
	}

	if (FD_ISSET(socket_, &rset) || FD_ISSET(socket_, &wset)) {
		len = sizeof(error);
		if (::getsockopt(socket_, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
			return(-1);			/* Solaris pending error */
	} else
        SPDLOG_ERROR("Select error: sockfd not set");

done:
	::fcntl(socket_, F_SETFL, flags);	/* restore file status flags */

	if (error) {
		::close(socket_);		/* just in case */
		errno = error;
		return(-1);
	}
    SPDLOG_INFO("Connected to PORT: {}", port_);
    is_connected = true;
	return(0);
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

std::string NetClient::get_response() {
    if (is_connected) {
        int nread = ::recv(socket_, buf_, buflen_, 0);
        if (nread < 0) {
            if (errno == EINTR) {
                // the socket call was interrupted -- try again
                return {};
            } else {
                // an error occurred, so break out
                return {};
            }
        } else if (nread == 0) {
            // the socket is closed
            return {};
        }
        // be sure to use append in case we have binary data
        return std::string(buf_);
    }
    return {};
}
