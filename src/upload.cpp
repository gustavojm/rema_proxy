#include "multipart.h"
#include "upload.hpp"
#include "session.hpp"

extern Session current_session;

void file_upload_handler(const std::shared_ptr<restbed::Session> &session) {
    const auto request = session->get_request();
    std::string asset = request->get_path_parameter("asset", "");

    size_t content_length = request->get_header("Content-Length", 0);

    session->fetch(content_length,   
            [&, asset](const std::shared_ptr<restbed::Session> &rest_session_ptr,
                    const restbed::Bytes &body) {

                std::string buffer(body.begin(), body.end());
                multipart::message multipart_msg(request->get_headers(), buffer);
                std::cout << multipart_msg.dump();

                for (auto part : multipart_msg.parts) {
                    for (auto header : part.headers) {
                        for (auto param : header.params) {
                            if (param.first == "filename") {
                                if (asset == "plans") {
                                    std::filesystem::path filename(param.second);
                                    std::istringstream istream(part.body); // this is an input stream
                                    if (current_session.is_loaded()) {
                                        current_session.load_plan(filename.replace_extension(), istream);
                                        std::cout << "created: " << filename.replace_extension() << "\n";
                                        current_session.save_to_disk();                                        
                                    }
                                } 

                                if (asset == "HXs") {
                                }
                            }
                        }
                    }
                }

                std::string stream = "File uploaded correctly";
                rest_session_ptr->close(restbed::OK, stream, { { "Content-Length",
                                std::to_string(stream.length()) }, {
                                "Content-Type",
                                "application/json; charset=utf-8" } });

            });
}


// @formatter:off
void upload_create_endpoints(restbed::Service &service) {

    auto resource_upload = std::make_shared<restbed::Resource>();
    resource_upload->set_path("/REMA/upload/{asset: .*}");
    // resource_upload->set_failed_filter_validation_handler(
    //         failed_filter_validation_handler);
    resource_upload->set_method_handler("POST", file_upload_handler);

    service.publish(resource_upload);
}

