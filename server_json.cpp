#include <memory>
#include <string>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <restbed>
#include <streambuf>

#include <iostream>
#include <asio.hpp>

using namespace std;
using namespace restbed;

#define MAX 80
#define SA struct sockaddr

void get_method_handler(const shared_ptr<Session> session)
{
	const auto request = session->get_request();
	const string filename = request->get_path_parameter("filename");

	ifstream stream("./wwwroot/" + filename, ifstream::in);

	if (stream.is_open()) {
		const string body = string(istreambuf_iterator<char>(stream),
				istreambuf_iterator<char>());

		const multimap<string, string> headers {
				{ "Content-Type", "text/html" }, { "Content-Length",
						::to_string(body.length()) } };

		session->close(OK, body, headers);
	} else {
		session->close(NOT_FOUND);
	}
}

void post_json_method_handler(const shared_ptr<Session> session)
{
	const auto request = session->get_request();

	size_t content_length = request->get_header("Content-Length", 0);

	session->fetch(content_length,
			[](const shared_ptr<Session> &session, const Bytes &body) {

				asio::io_service io_service;
				asio::ip::tcp::socket socket(io_service);	//socket creation

					socket.connect(
							asio::ip::tcp::endpoint(
									asio::ip::address::from_string("192.168.2.24"),
									1234));//connection

					asio::error_code error;
					asio::write(socket, asio::buffer(string( body.begin( ), body.end( ) )), error);
					if (!error) {
						cout << "Client sent hello message!" << endl;
					} else {
						cout << "send failed: " << error.message() << endl;
					}
					// getting response from server
					asio::streambuf receive_buffer;
					//asio::read(socket, receive_buffer, asio::transfer_all(), error);
					asio::read_until(socket, receive_buffer, "\0", error);
					if (error && error != asio::error::eof) {
						cout << "receive failed: " << error.message() << endl;
					} else {
						const char *data = asio::buffer_cast<const char*>(
								receive_buffer.data());
						cout << data << endl;
					}

					std::string stream ( (std::istreambuf_iterator<char>(&receive_buffer)), std::istreambuf_iterator<char>() );

					if (!stream.empty())
					stream.pop_back();// Erase null character at the end of stream response

					session->close(OK, stream,
							{	{	"Content-Length", ::to_string(stream.length())}, {
									"Content-Type",
									"application/json; charset=utf-8"}});
				});
}

void failed_filter_validation_handler(const shared_ptr<Session> session)
{
	const auto request = session->get_request();
	auto headers = request->get_headers();
	std::cout << "invalid: " << std::endl;
	for (auto h : headers) {
		cout << h.first << " - " << h.second << endl ;
	}

	session->close(400);
}

int main(const int, const char**)
{
	auto resource_json = make_shared<Resource>();
	resource_json->set_path("/json");
	resource_json->set_failed_filter_validation_handler(
			failed_filter_validation_handler);
//	resource_json->set_method_handler("POST", {
//			{ "Accept", "application/json" }, { "Content-Type",
//					"application/json" } }, post_json_method_handler);
	resource_json->set_method_handler("POST", post_json_method_handler);

	auto resource_html_file = make_shared<Resource>();
	resource_html_file->set_path("/static/{filename: [a-z]*\\.html}");
	resource_html_file->set_method_handler("GET", get_method_handler);

	auto settings = make_shared<Settings>();
	settings->set_port(1984);
	settings->set_default_header("Connection", "close");

	Service service;
	service.publish(resource_json);
	service.publish(resource_html_file);
	service.start(settings);

	return EXIT_SUCCESS;
}

