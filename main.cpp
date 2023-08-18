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
#include <thread>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include "inc/syslogger.hpp"
#include "inc/json.hpp"
#include "inc/csv.h"
#include "inc/net_commands.hpp"
#include "inc/inspection-session.hpp"
#include "websocket-server.hpp"
#include "rema.hpp"

InspectionSession current_session;

vector<shared_ptr<restbed::Session>> sessions;

using namespace std::chrono_literals;

std::map<std::string, std::string> mime_types = { { ".jpg", "image/jpg" }, {
        ".png", "image/png" }, { "svg", "image/svg+xml" },
        { ".css", "text/css" }, { ".js", "text/javascript" }, { ".ico",
                "image/x-icon" } };

void register_event_source_handler(const shared_ptr<restbed::Session> session) {
    const auto headers = multimap<string, string> {
            { "Connection", "keep-alive" }, { "Cache-Control", "no-cache" }, {
                    "Content-Type", "text/event-stream" }, {
                    "Access-Control-Allow-Origin", "*" } //Only required for demo purposes.
    };

    session->yield(OK, headers, [](const shared_ptr<restbed::Session> session) {
        sessions.push_back(session);
    });
}

void event_stream_handler() {
    static auto prev = std::chrono::high_resolution_clock::from_time_t(0);

    REMA &rema_instance = REMA::get_instance();
    static bool hide_sent = false;
    nlohmann::json res;

    // Using std::bind to bind the member function to an instance
    // auto bound_member_function = std::bind(&REMA::update_telemetry_callback_method, &rema_instance, std::placeholders::_1);

    try {
        rema_instance.rtu.receive_telemetry([&rema_instance](auto &rx_buffer) {rema_instance.update_telemetry_callback_method(rx_buffer); });
        res["TELEMETRY"] = rema_instance.telemetry;

        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed_time = now - prev;
        if (elapsed_time > std::chrono::milliseconds(500)) {
            res["TEMP_INFO"] = rema_instance.temps;
        }
    } catch (std::exception &e) {
        std::cerr << "Telemetry connection lost..." << "\n";
    }

    if (!rema_instance.rtu.isConnected) {
        res["SHOW_CONNECT"] = true;
        hide_sent = false;
    } else {
        if (!hide_sent) {
            res["HIDE_CONNECT"] = true;
            hide_sent = true;
        }
    }

    if (current_session.is_loaded() && current_session.is_changed()) {
        current_session.save_to_disk();
        current_session.set_changed(false);
        res["SESSION_MSG"] = "Session Saved";
    }

    if (!res.empty()) {
        sessions.erase(
                std::remove_if(sessions.begin(), sessions.end(),
                        [](const shared_ptr<Session> &a) {
                            return a->is_closed();
                        }),
                sessions.end());

        const auto message = "data: " + nlohmann::to_string(res) + "\n\n";
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

void post_rtu_method_handler(const shared_ptr<restbed::Session> session,
        REMA &rema) {
    const auto request = session->get_request();

    size_t content_length = request->get_header("Content-Length", 0);

    session->fetch(content_length,
            [&](const shared_ptr<restbed::Session> &session,
                    const Bytes &body) {

                std::string tx_buffer(body.begin(), body.end());

                boost::asio::streambuf rx_buffer;
                try {
                    rema.rtu.send(tx_buffer);

                    rema.rtu.receive([&session](auto &rx_buffer) {
                        std::string stream(
                                boost::asio::buffer_cast<const char*>(
                                        (rx_buffer).data()));
                        cout << stream << endl;

                        if (!stream.empty())
                            //stream.pop_back(); // Erase null character at the end of stream response

                            session->close(OK, stream, { { "Content-Length",
                                    ::to_string(stream.length()) }, {
                                    "Content-Type",
                                    "application/json; charset=utf-8" } });

                    });

                } catch (std::exception &e) {
                    std::string message = std::string(e.what());
                    std::cerr << "COMMUNICATIONS ERROR " << message << "\n";
                    session->close(OK, message, { { "Content-Length",
                            ::to_string(message.length()) }, { "Content-Type",
                            "application/json; charset=utf-8" } });
                }
            });
}

void post_json_method_handler(const shared_ptr<restbed::Session> session) {
    const auto request = session->get_request();
    size_t content_length = request->get_header("Content-Length", 0);

    session->fetch(content_length,
            [&](const shared_ptr<restbed::Session> &session,
                    const Bytes &body) {

                std::string b = { body.begin(), body.end() };

                nlohmann::json j;

                if (!b.empty()) {
                    // std::cout << b << std::endl;
                    try {
                        j = nlohmann::json::parse(b);
                    } catch (std::exception &e) {
                        std::cout << e.what() << "\n";
                    }
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

void failed_filter_validation_handler(
        const shared_ptr<restbed::Session> session) {
    const auto request = session->get_request();
    auto headers = request->get_headers();
    std::cout << "invalid: " << std::endl;
    for (auto h : headers) {
        cout << h.first << " - " << h.second << endl;
    }

    session->close(400);
}

int main(const int, const char**) {
    int rtu_proxy_port;

    REMA &rema_instance = REMA::get_instance();

    namespace po = boost::program_options;

    try {
        po::options_description json_proxy_settings("JSON Proxy Settings");
        json_proxy_settings.add_options()("JSON_PROXY.port",
                po::value<int>(&rtu_proxy_port)->default_value(1980),
                "Port number");

        std::string rtu_ip;
        int rtu_port;
        po::options_description rtu_settings("RTU Settings");
        rtu_settings.add_options()("RTU.ip",
                po::value<std::string>(&rtu_ip)->default_value("192.168.2.20"),
                "IP Address")("RTU.port",
                po::value<int>(&rtu_port)->default_value(5020), "Port Number");

        po::options_description config_file_settings;
        config_file_settings.add(json_proxy_settings).add(rtu_settings);

        po::variables_map vm;
        po::store(
                po::parse_config_file<char>("config.ini", config_file_settings),
                vm);
        po::notify(vm);

        rema_instance.rtu.set_ip(rtu_ip);
        rema_instance.rtu.set_port(rtu_port);

        std::cout << "REMA Proxy Server running on " << rtu_proxy_port << "\n";
        std::cout << "Connecting to RTU on " << rema_instance.rtu.get_ip() << ":"
                << rema_instance.rtu.get_port() << "\n";

        rema_instance.rtu.connect_comm();
        rema_instance.rtu.connect_telemetry();

    } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
    }

    using namespace std::placeholders;
    auto post_rtu_method_handler_bound = std::bind(post_rtu_method_handler,
            _1, std::ref(rema_instance)); // std::bind always passes by value unles std::ref

    auto resource_rtu = make_shared<restbed::Resource>();
    resource_rtu->set_path("/REMA");
    resource_rtu->set_failed_filter_validation_handler(
            failed_filter_validation_handler);
    resource_rtu->set_method_handler("POST", post_rtu_method_handler_bound);

    auto resource_json = make_shared<restbed::Resource>();
    resource_json->set_path("/json/{command: .*}");
    resource_json->set_failed_filter_validation_handler(
            failed_filter_validation_handler);
    resource_json->set_method_handler("POST", post_json_method_handler);

    auto resource_html_file = make_shared<restbed::Resource>();

    resource_html_file->set_paths( {
            "/static/{filename: ^.+\\.(html|jpg|png|svg|ico)$}",
            "/static/css/{filename: ^.+\\.(css)$}",
            "/static/js/{filename: ^.+\\.(js)$}",
            "/static/css/images/{filename: ^.+\\.(jpg|png)$}",
            "/static/images/{filename: ^.+\\.(jpg|png)$}", });

    resource_html_file->set_method_handler("GET", get_method_handler);

    auto resource_HXs = make_shared<restbed::Resource>();
    resource_HXs->set_path("/HXs/{folder: .*}/{file: .*}");
    resource_HXs->set_failed_filter_validation_handler(
            failed_filter_validation_handler);
    resource_HXs->set_method_handler("GET", get_HXs_method_handler);

    auto settings = make_shared<restbed::Settings>();
    settings->set_port(rtu_proxy_port);
    settings->set_default_header("Connection", "close");
    settings->set_worker_limit(std::thread::hardware_concurrency());

    auto resource_server_side_events = make_shared<restbed::Resource>();
    resource_server_side_events->set_path("/sse");
    resource_server_side_events->set_method_handler("GET",
            register_event_source_handler);

    restbed::Service service;
    service.publish(resource_rtu);
    service.publish(resource_HXs);
    service.publish(resource_json);
    service.publish(resource_html_file);
    service.publish(resource_server_side_events);

    service.schedule(event_stream_handler, std::chrono::milliseconds(100));

    service.set_logger(make_shared<SyslogLogger>());

//    // Websocket
//    std::thread websocket_thread(websocket_init);

    service.start(settings);

//    websocket_thread.join();

}

