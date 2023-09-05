#ifndef CIAA_HPP
#define CIAA_HPP

#include <string>
#include <restbed>
#include <thread>
#include <chrono>
#include <boost/asio.hpp>
#include <boost/asio/high_resolution_timer.hpp>

using boost::asio::ip::tcp;

struct IO_Service {
	using error_code = boost::system::error_code;

	template<typename AllowTime, typename Cancel> void await_operation_ex(
			AllowTime const &deadline_or_duration, Cancel &&cancel) {
		using namespace boost::asio;

		ioservice.reset();
		{
			high_resolution_timer tm(ioservice, deadline_or_duration);
			tm.async_wait([&cancel](error_code ec) {
				if (ec != error::operation_aborted)
					std::forward<Cancel>(cancel)();
			});
			ioservice.run_one();
		}
		ioservice.run();
	}

	template<typename AllowTime, typename ServiceObject> void await_operation(
			AllowTime const &deadline_or_duration, ServiceObject &so) {
		return await_operation_ex(deadline_or_duration, [&so] {
			so.cancel();
		});
	}

	boost::asio::io_service ioservice;
};

class CIAA {
public:
	void set_ip(const std::string ip) {
		this->ip = ip;
	}

	void set_port(int port) {
		this->port = port;
	}

	std::string get_ip() {
		return ip;
	}

	int get_port() {
		return port;
	}

	void connect_comm();
	void receive(std::function<void(boost::asio::streambuf &rx_buffer)> callback);
	void send(const std::string &tx_buffer);
	void send_sync(const std::string &tx_buffer);

	void connect_telemetry();
	void receive_telemetry(std::function<void(boost::asio::streambuf &rx_buffer)> callback);
	size_t receive_telemetry_sync(boost::asio::streambuf &rx_buffer);

private:
	std::string ip;
	int port;
	struct IO_Service serv;
	std::unique_ptr<tcp::socket> socket_comm;
	std::unique_ptr<tcp::socket> socket_telemetry;

public:
	bool isConnected = false;
};

#endif 		// CIAA_HPP
