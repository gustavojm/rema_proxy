#ifndef NET_CLIENT_HPP
#define NET_CLIENT_HPP

#include <boost/asio.hpp>
#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/io_context.hpp>
#include <chrono>
#include <restbed>
#include <string>
#include <thread>

using boost::asio::ip::tcp;

class netClient {
  public:
    void set_host(const std::string host_) {
        host = host_;
    }

    void set_service(std::string service_port) {
        service = service_port;
    }

    std::string get_host() {
        return host;
    }

    std::string get_service() {
        return service;
    }

    void connect();

    void receive_async(std::function<void(std::string &rx_buffer)> callback);

    std::string receive_blocking();

    void send_blocking(const std::string &line);

  private:
    std::chrono::steady_clock::duration timeout = std::chrono::milliseconds(1000);
    std::string host;
    std::string service;
    boost::asio::io_context io_context_;
    tcp::socket socket_{ io_context_ };
    std::string input_buffer_;
    std::mutex mtx;

    void run() {
        // Restart the io_context, as it may have been left in the "stopped" state
        // by a previous operation.
        io_context_.restart();

        // Block until the asynchronous operation has completed, or timed out. If
        // the pending asynchronous operation is a composed operation, the deadline
        // applies to the entire operation, rather than individual operations on
        // the socket.
        io_context_.run_for(timeout);

        // If the asynchronous operation completed successfully then the io_context
        // would have been stopped due to running out of work. If it was not
        // stopped, then the io_context::run_for call must have timed out.
        if (!io_context_.stopped()) {
            // Close the socket to cancel the outstanding asynchronous operation.
            socket_.close();

            // Run the io_context again until the operation completes.
            io_context_.run();
        }
    }

  public:
    bool isConnected = false;
};

#endif // CIAA_HPP
