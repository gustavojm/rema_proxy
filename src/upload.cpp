#include "multipart.hpp"
#include "upload.hpp"
#include "session.hpp"
#include "HX.hpp"

extern Session current_session;

void extract_plans_from_multipart_form_data(multipart::message &multipart_msg) {
    for (auto part : multipart_msg.parts) {
        for (auto header : part.headers) {
            for (auto [key, value] : header.params) {
                if (key == "filename" && !value.empty()) {
                    std::filesystem::path filename(value);
                    std::istringstream istream(part.body); // this is an input stream
                    if (current_session.is_loaded()) {
                        std::string plan_name = filename.replace_extension().string().substr(0, 25);
                        current_session.load_plan(plan_name, istream);
                        std::cout << "Added: " << plan_name << "\n";
                        current_session.save_to_disk();
                    }
                }
            }
        }
    }
}

void extract_HX_from_multipart_form_data(multipart::message &multipart_msg) {
    std::string HXname;
    std::string csv_content;
    std::string config_content;
    for (auto part : multipart_msg.parts) {
        for (auto header : part.headers) {
            for (auto [key, value] : header.params) {
                if (key == "name" && value == "tubesheet") {
                    if (auto it = header.params.find("filename"); it != header.params.end()) {
                        HXname = std::filesystem::path(it->second).replace_extension().string().substr(0, 25);
                        HX temp;
                        std::istringstream istream(part.body); // this is an input stream
                        temp.process_csv(HXname, istream);
                        csv_content = part.body;
                    }
                }
                if (key == "name" && value == "config") {
                    if (auto it = header.params.find("filename"); it != header.params.end()) {
                        if (it->second == "config.json"){
                            std::cout << part.body << "\n";
                            nlohmann::json config = nlohmann::json::parse(part.body, nullptr, true, true);
                            config_content = part.body;
                        }
                    }
                }
            }
        }
    }

    if (!HXname.empty() and !csv_content.empty() and !config_content.empty()) {
        HX::create(HXname, csv_content, config_content);
    }
}

void file_upload_handler(const std::shared_ptr<restbed::Session> &session) {
    const auto request = session->get_request();
    std::string asset = request->get_path_parameter("asset", "");
    size_t content_length = request->get_header("Content-Length", 0);

    session->fetch(content_length,   
            [&, asset](const std::shared_ptr<restbed::Session> &rest_session_ptr,
                    const restbed::Bytes &body) {
                nlohmann::json res;
                int status = restbed::OK;
                std::string buffer(body.begin(), body.end());
                multipart::message multipart_msg(request->get_headers(), buffer);
                //std::cout << multipart_msg.dump();

                try {
                    if (asset == "plans") {
                        extract_plans_from_multipart_form_data(multipart_msg);
                    }
                    if (asset == "HXs") {
                        extract_HX_from_multipart_form_data(multipart_msg);
                    }
                    res["success"] = "Uploaded correctly";
                } catch (std::exception &e) {
                    res["error"] = e.what();
                    status = restbed::BAD_REQUEST;
                }

                rest_session_ptr->close(status, res.dump(), { { "Content-Length",
                                std::to_string(res.dump().length()) }, {
                                "Content-Type",
                                "application/json; charset=utf-8" } });

            });
}

// @formatter:off
void upload_create_endpoints(restbed::Service &service) {

    auto resource_upload = std::make_shared<restbed::Resource>();
    resource_upload->set_path("/upload/{asset: .*}");
    // resource_upload->set_failed_filter_validation_handler(
    //         failed_filter_validation_handler);
    resource_upload->set_method_handler("POST", file_upload_handler);

    service.publish(resource_upload);
}

