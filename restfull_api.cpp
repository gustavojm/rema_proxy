#include <functional>
#include <ctime>
#include <thread>
#include <chrono>
#include <fstream>
#include <iostream>
#include <exception>
#include <vector>
#include <memory>

#include "json.hpp"
#include "inspection-session.hpp"
#include "tool.hpp"
#include "rema.hpp"
#include "HXs.hpp"

extern InspectionSession current_session;

struct ResourceEntry {
    std::string method;
    std::function<void (const std::shared_ptr<restbed::Session>)> callback;
};


/**
 * REMA related functions
 **/
void REMA_connect(const std::shared_ptr<restbed::Session> session) {
    std::string res_string;
    int status;
    try {
        REMA &rema_instance = REMA::get_instance();
        rema_instance.rtu.connect_comm();
        rema_instance.rtu.connect_telemetry();
        status = restbed::OK;
    } catch (std::exception &e) {
        res_string = e.what();
        status = restbed::INTERNAL_SERVER_ERROR;
    }
    session->close(status, res_string, { { "Content-Type", "application/json ; charset=utf-8" }, { "Content-Length", "0"} });
}

void REMA_info(const std::shared_ptr<restbed::Session> session) {
    nlohmann::json res = nlohmann::json(nlohmann::json::value_t::object);
    REMA &rema_instance = REMA::get_instance();

    res["tools"] = REMA::tools_list();
    res["last_selected_tool"] = rema_instance.last_selected_tool;
    std::string res_string = res.dump();
    session->close(restbed::OK, res_string, { { "Content-Type", "application/json ; charset=utf-8" }, { "Content-Length", std::to_string(res_string.length())} });
}

/**
 * HX related functions
 **/
void HXs_list_(const std::shared_ptr<restbed::Session> session) {
    std::string res_string = nlohmann::json(HXs_list()).dump();
    session->close(restbed::OK, res_string, { { "Content-Type", "application/json ; charset=utf-8" }, { "Content-Length", std::to_string(res_string.length())} });
}

void HXs_tubesheet_load(const std::shared_ptr<restbed::Session> session) {
    std::string res_string = nlohmann::json(HX_get_tubes(current_session.tubesheet_csv)).dump();
    session->close(restbed::OK, res_string, { { "Content-Type", "application/json ; charset=utf-8" }, { "Content-Length", std::to_string(res_string.length())} });
}

/**
 * Inspection Plans related functions
 **/

void inspection_plans(const std::shared_ptr<restbed::Session> session) {

    const auto request = session->get_request();
    std::string insp_plan = request->get_path_parameter("insp_plan", "");

    nlohmann::json res;
    if (!insp_plan.empty()) {
         res = current_session.inspection_plan_get(insp_plan);
    }
    std::string res_string = res.dump();
    session->close(restbed::OK, res_string, { { "Content-Type", "application/json ; charset=utf-8" }, { "Content-Length", std::to_string(res_string.length())} });
}


/**
 * Tools related functions
 **/
void tools_list(const std::shared_ptr<restbed::Session> session ) {
    nlohmann::json res = REMA::tools_list();
    std::string res_string = res.dump();
    session->close(restbed::OK, res_string, { { "Content-Type", "application/json ; charset=utf-8" }, { "Content-Length", std::to_string(res_string.length())} });
}

void tools_create(const std::shared_ptr<restbed::Session> session) {
    const auto request = session->get_request();
    size_t content_length = request->get_header("Content-Length", 0);
    session->fetch(content_length,
            [&](const std::shared_ptr<restbed::Session> &session,
                    const restbed::Bytes &body) {

                nlohmann::json form_data = nlohmann::json::parse(body.begin(), body.end());
                std::string res_string;
                int status;

                std::string tool_name = form_data["tool_name"];
                if (tool_name.empty()) {
                    std::string res_string = "No tool name specified";
                }

                try {
                    float offset_x = std::stof(form_data["offset_x"].get<std::string>());
                    float offset_y = std::stof(form_data["offset_y"].get<std::string>());
                    float offset_z = std::stof(form_data["offset_z"].get<std::string>());

                    Tool new_tool(tool_name, offset_x, offset_y, offset_z);
                    res_string = "Tool created Successfully";
                    status = restbed::CREATED;
                } catch (const std::exception &e) {
                    res_string = e.what();
                    status = restbed::INTERNAL_SERVER_ERROR;
                }

                session->close(status, res_string, { { "Content-Type", "application/json ; charset=utf-8" }, { "Content-Length", std::to_string(res_string.length())} });
    });
}

void tools_delete(const std::shared_ptr<restbed::Session> session) {
    const auto request = session->get_request();
    std::string tool_name = request->get_path_parameter("tool_name", "");

    try {
        REMA::delete_tool(tool_name);
        session->close(restbed::NO_CONTENT, "", { { "Content-Length", "0"} });
    } catch (const std::filesystem::filesystem_error &e) {
        std::string res_string = "Failed to delete the tool";
        session->close(restbed::INTERNAL_SERVER_ERROR, res_string, { { "Content-Length", std::to_string(res_string.length())} });
    }
}

void tools_select(const std::shared_ptr<restbed::Session> session) {
    auto request = session->get_request();
    std::string tool_name = request->get_path_parameter("tool_name", "");
    REMA &rema_instance = REMA::get_instance();
    rema_instance.set_selected_tool(tool_name);
    session->close(restbed::OK, "", { { "Content-Length", "0"} });
}


/**
 * Inspection Sessions related functions
 **/

void inspection_sessions_list(const std::shared_ptr<restbed::Session> session) {
    std::string res_string = nlohmann::json(InspectionSession::sessions_list()).dump();
    session->close(restbed::OK, res_string, { { "Content-Type", "application/json ; charset=utf-8" }, { "Content-Length", std::to_string(res_string.length())} });
}

void inspection_sessions_create(const std::shared_ptr<restbed::Session> session) {
    const auto request = session->get_request();
    size_t content_length = request->get_header("Content-Length", 0);
    session->fetch(content_length,
            [&](const std::shared_ptr<restbed::Session> &session,
                    const restbed::Bytes &body) {

                nlohmann::json form_data = nlohmann::json::parse(body.begin(), body.end());
                std::string session_name = form_data["session_name"];
                std::string res_string;
                int status = restbed::INTERNAL_SERVER_ERROR;

                if (session_name.empty()) {
                    res_string = "No filename specified";
                }

                if (form_data["hx"].empty()) {
                    res_string = "No HX specified";
                }

                try {
                    InspectionSession new_session(session_name, std::filesystem::path(form_data["hx"]));
                    res_string = new_session.load_plans();
                    new_session.save_to_disk();
                    current_session = new_session;
                    status = restbed::CREATED;
                } catch (const std::exception &e) {
                    res_string = e.what();
                }

                session->close(status, res_string, { { "Content-Type", "application/json ; charset=utf-8" }, { "Content-Length", std::to_string(res_string.length())} });
    });
}


void inspection_sessions_load(const std::shared_ptr<restbed::Session> session) {
    auto request = session->get_request();
    std::string session_name = request->get_path_parameter("session_name", "");

    std::string res_string;
    int status;
    if (!session_name.empty()) {
        try {
            current_session.load(session_name);
            res_string = nlohmann::json().dump();
            status = restbed::OK;
        } catch (std::exception &e) {
            res_string = e.what();
            status = restbed::INTERNAL_SERVER_ERROR;
        }
    } else {
        res_string = "No session selected";
        status = restbed::INTERNAL_SERVER_ERROR;
    }
    session->close(status, res_string, { { "Content-Type", "application/json ; charset=utf-8" }, { "Content-Length", std::to_string(res_string.length())} });
}

void inspection_sessions_info(const std::shared_ptr<restbed::Session> session) {
    nlohmann::json res = nlohmann::json::object();
    std::string res_string;

    if (current_session.is_loaded()) {
        res = current_session;
        res["is_loaded"] = true;
        res_string = res.dump();
    } else {
        res["is_loaded"] = false;
        res_string = res.dump();
    }
    session->close(restbed::OK, res_string, { { "Content-Type", "application/json ; charset=utf-8" }, { "Content-Length", std::to_string(res_string.length())} });
}


void insp_sessions_delete(const std::shared_ptr<restbed::Session> session) {
    auto request = session->get_request();
    std::string session_name = request->get_path_parameter("session_name", "");
    std::string res_string;
    int status = restbed::INTERNAL_SERVER_ERROR;
    try {
        InspectionSession::delete_session(session_name);
        res_string = nlohmann::json(current_session).dump();
        status = restbed::OK;
    } catch (const std::filesystem::filesystem_error &e) {
        res_string = std::string("filesystem error: ") + e.what();
        status = restbed::INTERNAL_SERVER_ERROR;
    }
    session->close(status, res_string, { { "Content-Type", "application/json ; charset=utf-8" }, { "Content-Length", std::to_string(res_string.length())} });
}

void cal_points_list(const std::shared_ptr<restbed::Session>) {
    return;
}

void cal_points_put(const std::shared_ptr<restbed::Session>) {
    return;
}

void cal_points_delete(const std::shared_ptr<restbed::Session>) {
    return;
}



void restfull_api_create_endpoints(restbed::Service &service) {
    std::map<std::string, std::vector<ResourceEntry>> rest_resources = {
        {"REMA/connect", { {"POST", &REMA_connect}}},
        {"REMA/info", { {"GET", &REMA_info}}},
        {"HXs", { {"GET", &HXs_list_}}},
        {"HXs/tubesheet/load", { {"GET", &HXs_tubesheet_load}}},
        {"inspection-plans", { {"GET", &inspection_plans}}},
        {"inspection-plans/{insp_plan: .*}", { {"GET", &inspection_plans}}},
        {"tools", {
                {"GET", &tools_list},
                {"POST", &tools_create},
            },
        },
        {"tools/{tool_name: .*}", {
                {"DELETE", &tools_delete},
            },
        },
        {"tools/{tool_name: .*}/select", {
                {"POST", &tools_select},
            },
        },
        {"inspection-sessions", {
                {"GET", &inspection_sessions_list},
                {"POST", &inspection_sessions_create},
                {"DELETE", &insp_sessions_delete},
            },
        },
        {"inspection-sessions/{session_name: .*}", {
                {"GET", &inspection_sessions_load},
            },
        },

        {"current-session/info", {
                {"GET", &inspection_sessions_info},
            },
        },

        {"calibration-points", {
                {"GET", &cal_points_list},
                {"PUT", &cal_points_put},
                {"DELETE", &cal_points_delete},
            },
        },
    };

    for (auto [path, resources] : rest_resources) {
        auto resource_rest = std::make_shared<restbed::Resource>();
        resource_rest->set_path(std::string("/REST/").append(path));
        //resource_rest->set_failed_filter_validation_handler(
        //        failed_filter_validation_handler);

        for (ResourceEntry r : resources) {
            resource_rest->set_method_handler(r.method, r.callback);
        }
        service.publish(resource_rest);
    }
}
