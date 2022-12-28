#include <memory>
#include <string>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <restbed>
#include <streambuf>
#include <functional>
#include <boost/variant/variant.hpp>
#include <boost/variant/get.hpp>

#include <iostream>
#include <asio.hpp>
#include <boost/program_options.hpp>

using namespace std;
using namespace restbed;

void get_method_handler(const shared_ptr<Session> session) {
	const auto request = session->get_request();
	const string filename = request->get_path_parameter("filename");

	ifstream stream("./wwwroot/" + filename, ifstream::in);

	if (stream.is_open()) {
		const string body = string(istreambuf_iterator<char>(stream),
				istreambuf_iterator<char>());

		string content_type = "text/html";

		string extension = filename.substr(filename.length() - 4, 4);

		if (extension == ".jpg") {
			content_type = "image/jpg";
		}
		if (extension == ".png") {
			content_type = "image/png";
		}

		const multimap<string, string> headers {
				{ "Content-Type", content_type }, { "Content-Length",
						::to_string(body.length()) } };

		session->close(OK, body, headers);
	} else {
		session->close(NOT_FOUND);
	}
}

asio::error_code send_to_ciaa(const asio::ip::address &ciaa_ip, int ciaa_port,
		const Bytes &tx_buffer, shared_ptr<asio::streambuf> rx_buffer) {
	asio::io_service io_service;
	asio::ip::tcp::socket socket(io_service); //socket creation
	socket.connect(asio::ip::tcp::endpoint(ciaa_ip, ciaa_port)); //connection
	asio::error_code error;
	asio::write(socket,
			asio::buffer(string(tx_buffer.begin(), tx_buffer.end())), error);
	if (error) {
		cout << "send failed: " << error.message() << endl;
		return error;
	}

	// getting response from server
	//asio::read(socket, receive_buffer, asio::transfer_all(), error);
	asio::read_until(socket, *rx_buffer, "\0", error);
	if (error && error != asio::error::eof) {
		cout << "receive failed: " << error.message() << endl;
		return error;
	}

	return asio::error_code();
}

void post_json_method_handler(const shared_ptr<Session> session,
		asio::ip::address ciaa_ip = asio::ip::address::from_string(
				"192.168.2.20"), int ciaa_port = 1234) {
	const auto request = session->get_request();

	size_t content_length = request->get_header("Content-Length", 0);

	session->fetch(content_length,
			[&](const shared_ptr<Session> &session, const Bytes &body) {

				shared_ptr<asio::streambuf> rx_buffer = make_shared<
						asio::streambuf>();
				auto ec = send_to_ciaa(ciaa_ip, ciaa_port, body, rx_buffer);

				if (ec) {
					session->close(OK, ec.message(),
							{ { "Content-Length", ::to_string(
									ec.message().length()) }, { "Content-Type",
									"application/json; charset=utf-8" } });
					//std::string stream ( (std::istreambuf_iterator<char>(&receive_buffer)), std::istreambuf_iterator<char>() );

				} else {
					std::string stream(
							asio::buffer_cast<const char*>(
									(*rx_buffer).data()));
					cout << stream << endl;

					if (!stream.empty())
						stream.pop_back(); // Erase null character at the end of stream response

					session->close(OK, stream, { { "Content-Length",
							::to_string(stream.length()) }, { "Content-Type",
							"application/json; charset=utf-8" } });
				}
			});
}

void failed_filter_validation_handler(const shared_ptr<Session> session) {
	const auto request = session->get_request();
	auto headers = request->get_headers();
	std::cout << "invalid: " << std::endl;
	for (auto h : headers) {
		cout << h.first << " - " << h.second << endl;
	}

	session->close(400);
}

int main(const int, const char**) {
	int JSON_proxy_port;
	std::string ciaa_ip;
	int ciaa_port;

	namespace po = boost::program_options;

	try {
		po::options_description json_proxy_settings("JSON Proxy Settings");
		json_proxy_settings.add_options()("JSON_PROXY.port",
				po::value<int>(&JSON_proxy_port)->default_value(1980),
				"Port number");

		po::options_description ciaa_settings("CIAA Settings");
		ciaa_settings.add_options()("CIAA.ip",
				po::value<std::string>(&ciaa_ip)->default_value("192.168.2.20"),
				"IP address")("CIAA.port",
				po::value<int>(&ciaa_port)->default_value(5020), "Port number");

		po::options_description config_file_settings;
		config_file_settings.add(json_proxy_settings).add(ciaa_settings);

		po::variables_map vm;
		po::store(
				po::parse_config_file<char>("config.ini", config_file_settings),
				vm);
		po::notify(vm);

		std::cout << "JSON Proxy Server running on " << JSON_proxy_port
				<< std::endl;
		std::cout << "Connecting to CIAA on " << ciaa_ip << ":" << ciaa_port
				<< std::endl;

	} catch (std::exception &e) {
		std::cout << e.what() << std::endl;
	}

	using namespace std::placeholders;
	auto post_json_method_handler_bound = std::bind(post_json_method_handler,
			_1, asio::ip::address::from_string(ciaa_ip), ciaa_port);

	auto resource_json = make_shared<Resource>();
	resource_json->set_path("/json");
	resource_json->set_failed_filter_validation_handler(
			failed_filter_validation_handler);
//	resource_json->set_method_handler("POST", {
//			{ "Accept", "application/json" }, { "Content-Type",
//					"application/json" } }, post_json_method_handler);
	resource_json->set_method_handler("POST", post_json_method_handler_bound);

	auto resource_html_file = make_shared<Resource>();
	//resource_html_file->set_path("/static/{filename: [a-z]*\\.html}");
	resource_html_file->set_path(
			"/static/{filename: ^.+\\.(html|css|jpg|png)$}");
	resource_html_file->set_method_handler("GET", get_method_handler);

	auto settings = make_shared<Settings>();
	settings->set_port(JSON_proxy_port);
	settings->set_default_header("Connection", "close");

	Service service;
	service.publish(resource_json);
	service.publish(resource_html_file);
	service.start(settings);

	return EXIT_SUCCESS;
}

