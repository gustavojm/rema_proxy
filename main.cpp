#include <memory>
#include <string>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <restbed>
#include <streambuf>
#include <functional>
#include <filesystem>
#include <map>
#include <exception>
#include <chrono>

#include <iostream>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include "syslogger.hpp"
#include "ciaa.hpp"
#include "json.hpp"
#include "csv.h"
#include "net_commands.hpp"
#include "insp_session.hpp"

using namespace std;
using namespace restbed;

inspection_session current_session;

vector<shared_ptr<Session> > sessions;

std::map<std::string, std::string> mime_types = { { ".jpg", "image/jpg" }, {
		".png", "image/png" }, { "svg", "image/svg+xml" },
		{ ".css", "text/css" }, { ".js", "text/javascript" }, { ".ico",
				"image/x-icon" } };

void register_event_source_handler(const shared_ptr<Session> session) {
	const auto headers = multimap<string, string> {
			{ "Connection", "keep-alive" }, { "Cache-Control", "no-cache" }, {
					"Content-Type", "text/event-stream" }, {
					"Access-Control-Allow-Origin", "*" } //Only required for demo purposes.
	};

	session->yield(OK, headers, [](const shared_ptr<Session> session) {
		sessions.push_back(session);
	});
}

void event_stream_handler(void) {

	if (current_session.is_changed()) {
		current_session.save_to_disk();
		current_session.set_changed(false);

		const auto message = "data: Session Saved\n\n";

		sessions.erase(
				std::remove_if(sessions.begin(), sessions.end(),
						[](const shared_ptr<Session> &a) {
							return a->is_closed();
						}),
				sessions.end());

		for (auto session : sessions) {
			session->yield(message);
		}

	}

}

void get_HXs_method_handler(const shared_ptr<Session> session) {
	const auto request = session->get_request();

	const string filename = request->get_path_parameter("filename");

	std::string path = request->get_path();
	if (path.front() == '/') {
		path.erase(path.begin());
	}

	std::filesystem::path f { path };
	ifstream stream(f, ifstream::in);

	if (stream.is_open()) {
		const string body = string(istreambuf_iterator<char>(stream),
				istreambuf_iterator<char>());

		std::string content_type;
		std::string ext = f.filename().extension();
		if (auto elem = mime_types.find(ext); elem != mime_types.end()) {
			content_type = (*elem).second;
		} else {
			content_type = "text/html";
		}

		const multimap<string, string> headers {
				{ "Content-Type", content_type }, { "Content-Length",
						::to_string(body.length()) } };

		session->close(OK, body, headers);
	} else {
		session->close(NOT_FOUND);
	}
}

void get_method_handler(const shared_ptr<Session> session) {
	const auto request = session->get_request();

	const string filename = request->get_path_parameter("filename");

	std::filesystem::path f { "./wwwroot/"
			+ request->get_path().substr(std::string("/static/").length()) };

	ifstream stream(f, ifstream::in);

	if (stream.is_open()) {
		const string body = string(istreambuf_iterator<char>(stream),
				istreambuf_iterator<char>());

		std::string content_type;
		std::string ext = f.filename().extension();
		if (auto elem = mime_types.find(ext); elem != mime_types.end()) {
			content_type = (*elem).second;
		} else {
			content_type = "text/html";
		}

		const multimap<string, string> headers {
				{ "Content-Type", content_type }, { "Content-Length",
						::to_string(body.length()) } };

		session->close(OK, body, headers);
	} else {
		session->close(NOT_FOUND);
	}
}

void post_ciaa_method_handler(const shared_ptr<Session> session, ciaa &ciaa) {
	const auto request = session->get_request();

	size_t content_length = request->get_header("Content-Length", 0);

	session->fetch(content_length,
			[&](const shared_ptr<Session> &session, const Bytes &body) {

				boost::asio::streambuf rx_buffer;
				try {
					ciaa.tx_rx(body, rx_buffer);
					std::string stream(
							boost::asio::buffer_cast<const char*>((rx_buffer).data()));
					cout << stream << endl;

					if (!stream.empty())
						//stream.pop_back(); // Erase null character at the end of stream response

						session->close(OK, stream, { { "Content-Length",
								::to_string(stream.length()) }, {
								"Content-Type",
								"application/json; charset=utf-8" } });

				} catch (std::exception &e) {
					std::string message = std::string(e.what());
					session->close(OK, message, { { "Content-Length",
							::to_string(message.length()) }, { "Content-Type",
							"application/json; charset=utf-8" } });
				}
			});
}

void post_json_method_handler(const shared_ptr<Session> session) {
	const auto request = session->get_request();

	size_t content_length = request->get_header("Content-Length", 0);

	session->fetch(content_length,
			[&](const shared_ptr<Session> &session, const Bytes &body) {

				std::string b = { body.begin(), body.end() };

				nlohmann::json j;

				try {
					j = nlohmann::json::parse(b);
				} catch (std::exception &e) {
					std::cout << e.what() << "\n";
				}
				const std::string command = request->get_path_parameter(
						"command");

				nlohmann::json res = cmd_execute(command, j);

				std::string res_string = res.dump();

				session->close(OK, res_string, { { "Content-Length",
						::to_string(res_string.length()) }, { "Content-Type",
						"application/json; charset=utf-8" } });
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
	int ciaa_proxy_port;

	ciaa &ciaa_instance = ciaa::get_instance();

	namespace po = boost::program_options;

	try {
		po::options_description json_proxy_settings("JSON Proxy Settings");
		json_proxy_settings.add_options()("JSON_PROXY.port",
				po::value<int>(&ciaa_proxy_port)->default_value(1980),
				"Port number");

		std::string ciaa_ip;
		int ciaa_port;
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

		ciaa_instance.set_ip(ciaa_ip);
		ciaa_instance.set_port(ciaa_port);

		std::cout << "JSON Proxy Server running on " << ciaa_proxy_port << "\n";
		std::cout << "Connecting to CIAA on " << ciaa_instance.get_ip() << ":"
				<< ciaa_instance.get_port() << "\n";

		try	{
				ciaa_instance.connect();
		} catch (std::exception &e) {
				std::cout << e.what() << std::endl;
		}

	} catch (std::exception &e) {
		std::cout << e.what() << std::endl;
	}

	using namespace std::placeholders;
	auto post_ciaa_method_handler_bound = std::bind(post_ciaa_method_handler,
			_1, std::ref(ciaa_instance)); // std::bind always passes by value unles std::ref

	auto resource_ciaa = make_shared<Resource>();
	resource_ciaa->set_path("/CIAA");
	resource_ciaa->set_failed_filter_validation_handler(
			failed_filter_validation_handler);
	resource_ciaa->set_method_handler("POST", post_ciaa_method_handler_bound);

	auto resource_json = make_shared<Resource>();
	resource_json->set_path("/json/{command: .*}");
	resource_json->set_failed_filter_validation_handler(
			failed_filter_validation_handler);
	resource_json->set_method_handler("POST", post_json_method_handler);

	auto resource_html_file = make_shared<Resource>();

	resource_html_file->set_paths( {
			"/static/{filename: ^.+\\.(html|jpg|png|svg|ico)$}",
			"/static/css/{filename: ^.+\\.(css)$}",
			"/static/js/{filename: ^.+\\.(js)$}",
			"/static/css/images/{filename: ^.+\\.(jpg|png)$}",
			"/static/images/{filename: ^.+\\.(jpg|png)$}", });

	resource_html_file->set_method_handler("GET", get_method_handler);

	auto resource_HXs = make_shared<Resource>();
	resource_HXs->set_path("/HXs/{folder: .*}/{file: .*}");
	resource_HXs->set_failed_filter_validation_handler(
			failed_filter_validation_handler);
	resource_HXs->set_method_handler("GET", get_HXs_method_handler);

	auto settings = make_shared<Settings>();
	settings->set_port(ciaa_proxy_port);
	settings->set_default_header("Connection", "close");

	auto resource_server_side_events = make_shared<Resource>();
	resource_server_side_events->set_path("/sse");
	resource_server_side_events->set_method_handler("GET",
			register_event_source_handler);

	Service service;
	service.publish(resource_ciaa);
	service.publish(resource_HXs);
	service.publish(resource_json);
	service.publish(resource_html_file);
	service.publish(resource_server_side_events);
	service.schedule(event_stream_handler, std::chrono::seconds(2));

	service.set_logger(make_shared<SyslogLogger>());
	service.start(settings);

	return EXIT_SUCCESS;
}

