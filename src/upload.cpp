#include "multipart.hpp"
#include "upload.hpp"
#include "session.hpp"
#include "HXs.hpp"

extern Session current_session;

void extract_plans_from_multipart_header(multipart::message &multipart_msg) {
    for (auto part : multipart_msg.parts) {
        for (auto header : part.headers) {
            for (auto param : header.params) {
                if (param.first == "filename") {
                    std::filesystem::path filename(param.second);
                    std::istringstream istream(part.body); // this is an input stream
                    if (current_session.is_loaded()) {
                        current_session.load_plan(filename.replace_extension(), istream);
                        std::cout << "created: " << filename.replace_extension() << "\n";
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
                        HXname = std::filesystem::path(it->second).replace_extension();
                        Session temp;
                        std::istringstream istream(part.body); // this is an input stream
                        temp.process_HXs_csv(it->second, istream);
                        csv_content = part.body;
                    }
                }
                if (key == "name" && value == "config") {
                    if (auto it = header.params.find("filename"); it != header.params.end()) {                        
                        config_content = nlohmann::json::parse(part.body, nullptr, true, true);
                    }
                }
            }
        }
    }

    if (!HXname.empty() and !csv_content.empty() and !config_content.empty()) {
        HXs_create(HXname, csv_content, config_content);
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
                // std::cout << multipart_msg.dump();

                try {
                    if (asset == "plans") {
                        extract_plans_from_multipart_header(multipart_msg);
                    }
                    if (asset == "HXs") {
                        extract_HX_from_multipart_form_data(multipart_msg);
                    }
                    res["success"] = "File uploaded correctly";
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

