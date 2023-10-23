#include <iostream>
#include <string>
#include <restbed>

#include <cstdlib>
#include <cstring>
#include <thread>
#include <chrono>
#include <boost/asio.hpp>
#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/io_context.hpp>
#include <json.hpp>
#include <net_client.hpp>
#include "debug.hpp"

using boost::asio::ip::tcp;

void netClient::connect() {
    // Resolve the host name and service to a list of endpoints.
    auto endpoints = tcp::resolver(io_context_).resolve(host, service);

    for (auto endpoint : endpoints) {
        lDebug(Info, "Connecting to: %s : %s", endpoint.host_name().c_str(), endpoint.service_name().c_str());
    }

    // Start the asynchronous operation itself. The lambda that is used as a
    // callback will update the error variable when the operation completes.
    // The blocking_udp_client.cpp example shows how you can use std::bind
    // rather than a lambda.
    boost::system::error_code error;
    boost::asio::async_connect(socket_, endpoints,
            [&](const boost::system::error_code &result_error,
                    const tcp::endpoint& /*result_endpoint*/) {
                error = result_error;
            });

    // Run the operation until it completes, or until the timeout.
    run();

    // Determine whether a connection was successfully established.
    if (error)
        throw std::system_error(error);

    isConnected = true;
}

void netClient::receive_async(std::function<void(std::string &rx_buffer)> callback) {
    // Start the asynchronous operation. The lambda that is used as a callback
    // will update the error and n variables when the operation completes. The
    // blocking_udp_client.cpp example shows how you can use std::bind rather
    // than a lambda.
    std::lock_guard<std::mutex> lock(mtx);
    boost::system::error_code error;
    std::size_t n = 0;
    boost::asio::async_read_until(socket_,
            boost::asio::dynamic_buffer(input_buffer_), '\0',
            [&](const boost::system::error_code &result_error,
                    std::size_t result_n) {
                error = result_error;
                n = result_n;

                if (error) {
                    isConnected = false;
                    throw std::runtime_error(
                            "Error receiving message: " + error.message());
                }

                //lDebug(Info, "Received message is: %s", input_buffer_.c_str());
                std::string line(input_buffer_.substr(0, n - 1));
                callback(line);
                input_buffer_.erase(0, n);
            });

    // Run the operation until it completes, or until the timeout.
    run();
    if (error) {
        isConnected = false;
        throw std::system_error(error);
    }
}

std::string netClient::receive_blocking() {
    // Start the asynchronous operation. The lambda that is used as a callback
    // will update the error and n variables when the operation completes. The
    // blocking_udp_client.cpp example shows how you can use std::bind rather
    // than a lambda.
    std::lock_guard<std::mutex> lock(mtx);
    boost::system::error_code error;
    std::size_t n = 0;
    boost::asio::async_read_until(socket_,
            boost::asio::dynamic_buffer(input_buffer_), '\0',
            [&](const boost::system::error_code &result_error,
                    std::size_t result_n) {
                error = result_error;
                n = result_n;
            });

    // Run the operation until it completes, or until the timeout.
    run();

    // Determine whether the read completed successfully.
    if (error) {
        isConnected = false;
        throw std::system_error(error);
    }

    std::string line(input_buffer_.substr(0, n - 1));
    input_buffer_.erase(0, n);
    return line;
}

void netClient::send_blocking(const std::string &line) {

    std::lock_guard<std::mutex> lock(mtx);
    std::string data = line + "\0";
    // Start the asynchronous operation itself. The lambda that is used as a
    // callback will update the error variable when the operation completes.
    // The blocking_udp_client.cpp example shows how you can use std::bind
    // rather than a lambda.
    boost::system::error_code error;
    boost::asio::async_write(socket_, boost::asio::buffer(data),
            [&](const boost::system::error_code &result_error,
                    std::size_t /*result_n*/) {
                error = result_error;
            });

    // Run the operation until it completes, or until the timeout.
    run();

    // Determine whether the read completed successfully.
    if (error) {
        isConnected = false;
        throw std::system_error(error);
    }
}
