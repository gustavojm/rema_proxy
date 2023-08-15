#include <iostream>
#include <string>
#include <restbed>

#include <cstdlib>
#include <cstring>
#include <thread>
#include <chrono>
#include <boost/asio.hpp>
#include <boost/asio/high_resolution_timer.hpp>
#include <ciaa.hpp>

constexpr std::chrono::milliseconds TIMEOUT = std::chrono::milliseconds(500);
using boost::asio::ip::tcp;

void CIAA::connect_comm() {
    socket_comm.reset(new tcp::socket(serv.ioservice));	// Create new socket (old one is destroyed automatically)
    tcp::resolver resolver(serv.ioservice);
    tcp::resolver::iterator endpoint_iter = resolver.resolve(ip, std::to_string(port));

    boost::asio::async_connect(*socket_comm, endpoint_iter,
            [&](boost::system::error_code ec, tcp::resolver::iterator it) {
                if (ec) {
                    isConnected = true;             // Should be "false" changed to allow reconnection if network was down initially
                    throw std::runtime_error(
                            "Error connecting to RTU: " + ec.message());
                } else {
                    isConnected = true;
//                    std::cout << "Connected to RTU: " << it->endpoint()
//                        << std::endl;
                }
            });
    serv.await_operation_ex(TIMEOUT, [&] {
        isConnected = false;
        throw std::runtime_error("Error connecting to RTU: timeout\n");
    });
}

void CIAA::connect_telemetry() {
    socket_telemetry.reset(new tcp::socket(serv.ioservice)); // Create new socket (old one is destroyed automatically)
    tcp::resolver resolver(serv.ioservice);

    tcp::resolver::iterator endpoint_iter = resolver.resolve(ip, std::to_string(port + 1));

    boost::asio::async_connect(*socket_telemetry, endpoint_iter,
            [&](boost::system::error_code ec, tcp::resolver::iterator it) {
                if (ec) {
                    throw std::runtime_error(
                            "Error connecting to Telemetry Socket: " + ec.message());
                } else {
//                    std::cout << "Connected to RTU: " << it->endpoint()
//                        << std::endl;
                }
            });
    serv.await_operation_ex(TIMEOUT, [&] {
        throw std::runtime_error("Error connecting to Telemetry Socket in RTU: timeout\n");
    });


}

size_t CIAA::receive(boost::asio::streambuf &rx_buffer) {
    size_t bytes;
    if (isConnected) {
        boost::asio::async_read_until(*socket_comm, rx_buffer, '\0',
                [&](boost::system::error_code ec, size_t bytes_transferred) {
                    if (ec) {
                        isConnected = false;
                        throw std::runtime_error(
                                "Error receiving message: " + ec.message());
                    }
                    //std::cout << "Received message is: " << &rx_buffer << '\n';
                    bytes = bytes_transferred;
                });

        serv.await_operation(TIMEOUT, *socket_comm);
    }
    return bytes;
}

void CIAA::send(const std::string &tx_buffer) {
    const restbed::Bytes tx_buffer_bytes(tx_buffer.begin(), tx_buffer.end());

    CIAA::connect_comm();

    if (isConnected) {
        boost::asio::async_write(*socket_comm, boost::asio::buffer(tx_buffer),
                [&](boost::system::error_code ec, size_t /*bytes_transferred*/) {
                    if (ec) {
                        isConnected = false;
                        throw std::runtime_error(
                                "Error sending message: " + ec.message());
                    }
                });
        serv.await_operation(TIMEOUT, *socket_comm);
    }
}

void CIAA::tx_rx(const std::string &tx_buffer,
    boost::asio::streambuf &rx_buffer) {
    send(tx_buffer);
    receive(rx_buffer);
}

size_t CIAA::receive_telemetry(boost::asio::streambuf &rx_buffer) {
    size_t bytes;
    boost::asio::async_read_until(*socket_telemetry, rx_buffer, '\0',
            [&](boost::system::error_code ec, size_t bytes_transferred) {
                if (ec) {
                    throw std::runtime_error(
                            "Error receiving message: " + ec.message());
                }
                //std::cout << "Received message is: " << &rx_buffer << '\n';
                bytes = bytes_transferred;
            });

    serv.await_operation(TIMEOUT, *socket_telemetry);
    return bytes;
}
