#ifndef TELEMETRY_NET_CLIENT_HPP
#define TELEMETRY_NET_CLIENT_HPP

#include <boost/asio.hpp>
#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/io_context.hpp>

#include <chrono>
#include <restbed>
#include <string>
#include <spdlog/spdlog.h>
#include <iostream>
#include <thread>
#include "net_client.hpp"

using boost::asio::ip::tcp;
using namespace std::placeholders;

class TelemetryNetClient : public netClient {
  public:

  TelemetryNetClient() {};

  void handle_connect(const boost::system::error_code& ec,
        tcp::resolver::iterator endpoint_iter) override {
      if (stopped_)
        return;

      // The async_connect() function automatically opens the socket at the start
      // of the asynchronous operation. If the socket is closed at this time then
      // the timeout handler must have run first.
      if (!socket_.is_open()) {
        SPDLOG_ERROR("Connect timed out");

        // Try the next available endpoint.
        start_connect(++endpoint_iter);
      }

      // Check if the connect operation failed before the deadline expired.
      else if (ec) {
        SPDLOG_ERROR("Connect error {}", ec.message());

        // We need to close the socket used in the previous connection attempt
        // before starting a new one.
        socket_.close();

        // Try the next available endpoint.
        start_connect(++endpoint_iter);
      }

      // Otherwise we have successfully established a connection.
      else {
        is_connected = true;
        std::cout << "Connected to " << endpoint_iter->endpoint() << "\n";
        // Start the input actor.        
        start_read();
      }
    };

  void start_read() {
    // Set a deadline for the read operation.
    deadline_.expires_from_now(boost::posix_time::seconds(30));

    // Start an asynchronous operation to read a newline-delimited message.
    boost::asio::async_read_until(
      socket_,
      input_buffer_,
     '\0',
     std::bind(&TelemetryNetClient::handle_read, this, std::placeholders::_1));
  }

  void handle_read(const boost::system::error_code& ec) {
    if (stopped_)
      return;

    if (!ec) {
      // Extract the newline-delimited message from the buffer.
      std::string line;
      std::istream is(&input_buffer_);
      std::getline(is, line);

      // Empty messages are heartbeats and so ignored.
      if (!line.empty()) {        
         lambda(line);
      }

      start_read();
    } else {
      SPDLOG_ERROR("Error on receive: {}", ec.message());
      stop();
    }
  }

  std::function<void(std::string)> lambda;

};

#endif // TELEMETRY_NET_CLIENT_HPP
