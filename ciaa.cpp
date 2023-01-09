#include <iostream>
#include <string>
#include <restbed>
#include "ciaa.hpp"


asio::error_code ciaa::tx_rx(const restbed::Bytes &tx_buffer, asio::streambuf& rx_buffer) {
	asio::io_service io_service;
	asio::ip::tcp::socket socket(io_service); //socket creation
	socket.connect(asio::ip::tcp::endpoint(ip, port)); //connection
	asio::error_code error;
	asio::write(socket,
			asio::buffer(std::string(tx_buffer.begin(), tx_buffer.end())), error);
	if (error) {
		std::cout << "send failed: " << error.message() << std::endl;
		return error;
	}

	// getting response from server
	//asio::read(socket, receive_buffer, asio::transfer_all(), error);
	asio::read_until(socket, rx_buffer, "\0", error);
	if (error && error != asio::error::eof) {
		std::cout << "receive failed: " << error.message() << std::endl;
		return error;
	}

	return asio::error_code();
}
