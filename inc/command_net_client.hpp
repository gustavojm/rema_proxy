#ifndef COMMAND_NET_CLIENT_HPP
#define COMMAND_NET_CLIENT_HPP

#include <boost/asio.hpp>
#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/lambda/lambda.hpp>

#include <chrono>
#include <restbed>
#include <spdlog/spdlog.h>
#include <string>
#include <iostream>
#include <thread>
#include "net_client.hpp"

using boost::asio::ip::tcp;

class CommandNetClient : public netClient {
  public:

  CommandNetClient() {};

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
        SPDLOG_ERROR("Connect error: {}", ec.message());

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

      }
    };

  std::string read_line() {
   try {
        std::string line;

        // Read data from the socket until we encounter the null character '\0'
        boost::asio::streambuf buffer;
        size_t reply_length = boost::asio::read_until(socket_, buffer, '\0');
        (void) reply_length;

        // Convert the streambuf into a string
        std::istream is(&buffer);
        std::getline(is, line, '\0');

        // Print the received data
        std::cout << "Received: " << line << std::endl;
        return line;
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return {};  
  }

  void write_line(const std::string& line) {
    boost::asio::write(socket_, boost::asio::buffer(line));
    std::cout << "Sending to REMA: " << line << "\n";
  }

  
};

#endif // COMMAND_NET_CLIENT_HPP
