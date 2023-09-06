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
#include "points.hpp"
#include "circle_fns.hpp"
#include "misc_fns.hpp"


extern InspectionSession current_session;


void close_session(const std::shared_ptr<restbed::Session>& session, int status, std::string res) {
    session->close(status, res, { { "Content-Type", "text/html ; charset=utf-8" }, { "Content-Length", std::to_string(res.length())} });
}

void close_session(const std::shared_ptr<restbed::Session>& session, int status, nlohmann::json json_res) {
    std::string res = json_res.dump();
    session->close(status, res, { { "Content-Type", "application/json ; charset=utf-8" }, { "Content-Length", std::to_string(res.length())} });
}

struct ResourceEntry {
    std::string method;
    std::function<void (const std::shared_ptr<restbed::Session>)> callback;
};

/**
 * REMA related functions
 **/
void REMA_connect(const std::shared_ptr<restbed::Session> session) {
    std::string res;
    int status;
    try {
        REMA &rema_instance = REMA::get_instance();
        rema_instance.command_client.connect();
        rema_instance.telemetry_client.connect();
        status = restbed::OK;
    } catch (std::exception &e) {
        res = e.what();
        status = restbed::INTERNAL_SERVER_ERROR;
    }
    close_session(session, status, res);
}

void REMA_info(const std::shared_ptr<restbed::Session> session) {
    nlohmann::json res = nlohmann::json(nlohmann::json::value_t::object);
    REMA &rema_instance = REMA::get_instance();

    res["tools"] = REMA::tools_list();
    res["last_selected_tool"] = rema_instance.last_selected_tool;
    close_session(session, restbed::OK, res);
}

/**
 * HX related functions
 **/
void HXs_list_(const std::shared_ptr<restbed::Session> session) {
    close_session(session, restbed::OK, nlohmann::json(HXs_list()));
}

void HXs_tubesheet_load(const std::shared_ptr<restbed::Session> session) {
    close_session(session, restbed::OK,  nlohmann::json(HX_get_tubes(current_session.tubesheet_csv)));
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
    close_session(session, restbed::OK, res);
}


/**
 * Tools related functions
 **/
void tools_list(const std::shared_ptr<restbed::Session> session ) {
    nlohmann::json res = REMA::tools_list();
    close_session(session, restbed::OK, res);
}

void tools_create(const std::shared_ptr<restbed::Session> session) {
    const auto request = session->get_request();
    size_t content_length = request->get_header("Content-Length", 0);
    session->fetch(content_length,
            [&](const std::shared_ptr<restbed::Session> &session,
                    const restbed::Bytes &body) {

                nlohmann::json form_data = nlohmann::json::parse(body.begin(), body.end());
                std::string res;
                int status;

                std::string tool_name = form_data["tool_name"];
                if (tool_name.empty()) {
                    res = "No tool name specified";
                }

                try {
                    if (!res.empty()) {
                        status = restbed::BAD_REQUEST;
                    } else {
                        float offset_x = to_float(form_data.value("offset_x", "0"));
                        float offset_y = to_float(form_data.value("offset_y", "0"));
                        float offset_z = to_float(form_data.value("offset_z", "0"));

                        Tool new_tool(tool_name, offset_x, offset_y, offset_z);
                        res = "Tool created Successfully";
                        status = restbed::CREATED;
                    }
                } catch (const std::invalid_argument &e) {
                    res += "Offsets wrong";
                    status = restbed::BAD_REQUEST;
                }
                catch (const std::exception &e) {
                        res = e.what();
                        status = restbed::INTERNAL_SERVER_ERROR;
                }
                close_session(session, status, res);
    });
}

void tools_delete(const std::shared_ptr<restbed::Session> session) {
    const auto request = session->get_request();
    std::string tool_name = request->get_path_parameter("tool_name", "");

    std::string res;
    int status;
    try {
        REMA::delete_tool(tool_name);
        status = restbed::NO_CONTENT;
    } catch (const std::filesystem::filesystem_error &e) {
        res = "Failed to delete the tool";
        status = restbed::INTERNAL_SERVER_ERROR;
    }
    close_session(session, status, res);
}

void tools_select(const std::shared_ptr<restbed::Session> session) {
    auto request = session->get_request();
    std::string tool_name = request->get_path_parameter("tool_name", "");
    REMA &rema_instance = REMA::get_instance();
    rema_instance.set_selected_tool(tool_name);
    close_session(session, restbed::OK, std::string(""));
}


/**
 * Inspection Sessions related functions
 **/

void inspection_sessions_list(const std::shared_ptr<restbed::Session> session) {
    nlohmann::json res;

    for (auto session : InspectionSession::sessions_list()) {
        res.push_back({{"name", session.name}, {"hx", session.hx }, {"last_write_time", session.last_write_time}});
    }
    close_session(session, restbed::OK, res);
}

void inspection_sessions_create(const std::shared_ptr<restbed::Session> session) {
    const auto request = session->get_request();
    size_t content_length = request->get_header("Content-Length", 0);
    session->fetch(content_length,
            [&](const std::shared_ptr<restbed::Session> &session,
                    const restbed::Bytes &body) {

                nlohmann::json form_data = nlohmann::json::parse(body.begin(), body.end());
                std::string res;
                int status;
                std::string session_name = form_data["session_name"];
                if (session_name.empty()) {
                    res += "No filename specified \n";
                }

                if (form_data["hx"].empty()) {
                    res += "No HX specified \n";
                }

                try {
                    if (!res.empty()) {
                        status = restbed::BAD_REQUEST;
                    } else {
                        InspectionSession new_session(session_name, std::filesystem::path(form_data["hx"]));
                        res = new_session.load_plans();
                        new_session.save_to_disk();
                        current_session = new_session;
                        status = restbed::CREATED;
                    }
                } catch (const std::exception &e) {
                    res += "Error creating InspectionSession \n";
                    status = restbed::INTERNAL_SERVER_ERROR;
                }
                close_session(session, status, res);
    });
}


void inspection_sessions_load(const std::shared_ptr<restbed::Session> session) {
    auto request = session->get_request();
    std::string session_name = request->get_path_parameter("session_name", "");

    std::string res;
    int status;
    if (!session_name.empty()) {
        try {
            current_session.load(session_name);
            status = restbed::OK;
        } catch (std::exception &e) {
            res = e.what();
            status = restbed::INTERNAL_SERVER_ERROR;
        }
    } else {
        res = "No session selected";
        status = restbed::INTERNAL_SERVER_ERROR;
    }
    close_session(session, restbed::OK, res);
}

void inspection_sessions_info(const std::shared_ptr<restbed::Session> session) {
    nlohmann::json res = nlohmann::json::object();
    if (current_session.is_loaded()) {
        res = current_session;
        res["aligned_tubes"] = current_session.calculate_aligned_tubes();
        res["is_loaded"] = true;
    } else {
        res["is_loaded"] = false;
    }
    close_session(session, restbed::OK, res);

}

void inspection_sessions_delete(const std::shared_ptr<restbed::Session> session) {
    auto request = session->get_request();
    std::string session_name = request->get_path_parameter("session_name", "");
    try {
        InspectionSession::delete_session(session_name);
        close_session(session, restbed::OK, nlohmann::json(current_session));
        return;
    } catch (const std::filesystem::filesystem_error &e) {
        std::string res = std::string("filesystem error: ") + e.what();
        close_session(session, restbed::INTERNAL_SERVER_ERROR, res);
    }
}

/**
 * Calibration Points related functions
 **/

void cal_points_list(const std::shared_ptr<restbed::Session> session) {
    close_session(session, restbed::OK, nlohmann::json(current_session.cal_points));
}

void cal_points_add(const std::shared_ptr<restbed::Session> session) {
    const auto request = session->get_request();
    size_t content_length = request->get_header("Content-Length", 0);
    session->fetch(content_length,
            [&](const std::shared_ptr<restbed::Session> &session,
                    const restbed::Bytes &body) {

                nlohmann::json form_data = nlohmann::json::parse(body.begin(), body.end());
                std::string res;
                int status;

                try {
                    std::string tube_id = form_data["tube_id"];
                    Point3D ideal_coords = {
                            to_float(form_data.value("ideal_coords_x", "0")),
                            to_float(form_data.value("ideal_coords_y", "0")),
                            to_float(form_data.value("ideal_coords_z", "0")),
                    };

                    Point3D determined_coords = {
                            to_float(form_data.value("determined_coords_x", "0")),
                            to_float(form_data.value("determined_coords_y", "0")),
                            to_float(form_data.value("determined_coords_z", "0")),
                    };
                    if (!tube_id.empty()) {
                        current_session.cal_points_add(tube_id, form_data["col"], form_data["row"], ideal_coords, determined_coords);
                        status = restbed::OK;
                    } else {
                        res += "No tool name specified";
                        status = restbed::BAD_REQUEST;
                    }
                } catch(std::exception &e) {
                    res += e.what();
                    status = restbed::INTERNAL_SERVER_ERROR;
                }
                close_session(session, status, res);
    });
};

void cal_points_update(const std::shared_ptr<restbed::Session> session) {
    const auto request = session->get_request();
    std::string tube_id = request->get_path_parameter("tube_id", "");

    size_t content_length = request->get_header("Content-Length", 0);
    session->fetch(content_length,
            [&](const std::shared_ptr<restbed::Session> &session,
                    const restbed::Bytes &body) {

                nlohmann::json form_data = nlohmann::json::parse(body.begin(), body.end());
                std::string res;
                int status;

                try {
                    Point3D determined_coords = {
                            to_float(form_data.value("determined_coords_x", "0")),
                            to_float(form_data.value("determined_coords_y", "0")),
                            0
                    };
                    if (!tube_id.empty()) {
                        current_session.cal_points_set_determined_coords(tube_id, determined_coords);
                        status = restbed::OK;
                    } else {
                        res += "No tool name specified";
                        status = restbed::BAD_REQUEST;
                    }
                } catch(std::exception &e) {
                    res += e.what();
                    status = restbed::INTERNAL_SERVER_ERROR;
                }
                close_session(session, status, res);
    });
};


void cal_points_delete(const std::shared_ptr<restbed::Session> session) {
    const auto request = session->get_request();
    std::string tube_id = request->get_path_parameter("tube_id", "");

    std::string res;
    int status;
    if (tube_id.empty()) {
        res = "No tube specified";
        status = restbed::INTERNAL_SERVER_ERROR;
    } else {
        current_session.cal_points_delete(tube_id);
        status = restbed::NO_CONTENT;
    }
    close_session(session, status, res);
}

void tubes_set_status(const std::shared_ptr<restbed::Session> session) {

    const auto request = session->get_request();
    std::string tube_id = request->get_path_parameter("tube_id", "");

    size_t content_length = request->get_header("Content-Length", 0);
    std::string session_name = request->get_path_parameter("session_name", "");

    session->fetch(content_length,
            [&](const std::shared_ptr<restbed::Session> &session,
                    const restbed::Bytes &body) {

                nlohmann::json form_data = nlohmann::json::parse(body.begin(), body.end());
                std::string res_string;

                std::string insp_plan = form_data["insp_plan"];
                bool checked = form_data["checked"];
                current_session.set_tube_inspected(insp_plan, tube_id, checked);
                nlohmann::json res = nlohmann::json::object();
                res[tube_id] = checked;
                close_session(session, restbed::OK, res);
    });
}

void determine_tube_center(const std::shared_ptr<restbed::Session> session) {
    REMA &rema_instance = REMA::get_instance();
    const auto request = session->get_request();
    std::string tube_id = request->get_path_parameter("tube_id", "");

    nlohmann::json res;

    if (!tube_id.empty()) {
        double scale = current_session.unit == "inch" ? 1 : 25.4;
        double tube_radius = (current_session.tube_od / 2) / scale;

        constexpr int points_number = 5;
        static_assert(points_number % 2 != 0, "Number of points must be odd");
        std::vector<Point3D> reordered_points;
        std::vector<Point3D> points = calculateCirclePoints(current_session.tubes[tube_id] / scale, tube_radius, points_number);

        int i = 0;
        for (int n = 0; n < points_number; n++) {
            reordered_points.push_back(points[i % points_number]);
            i += 2;
        }

        std::vector<sequence_step> seq;
        for (auto point : reordered_points) {
            sequence_step step;
            step.axes = "XY";
            step.first_axis_setpoint = point.x;
            step.second_axis_setpoint = point.y;
            step.stop_on_probe = true;
            step.stop_on_condition = true;

            seq.push_back(step);
        }

        rema_instance.execute_sequence(seq);

        std::vector<Point3D> tube_boundary_points;

        for (auto step : seq) {
            if (step.executed && step.execution_results.stopped_on_condition) {
                tube_boundary_points.push_back(step.execution_results.coords);
            }
        }
        res["reached_coords"] = tube_boundary_points;
        auto [center, radius] = fitCircle(tube_boundary_points);
        res["center"] = center;

        sequence_step goto_center_step;
        goto_center_step.axes = "XY";
        goto_center_step.first_axis_setpoint = center.x;
        goto_center_step.second_axis_setpoint = center.y;
        goto_center_step.stop_on_probe = true;
        goto_center_step.stop_on_condition = true;

        std::vector<sequence_step> goto_center_seq(1, goto_center_step);

        rema_instance.execute_sequence(goto_center_seq);

        res["radius"] = radius;
        close_session(session, restbed::OK, res);
    }
}

void aligned_tubesheet_get(const std::shared_ptr<restbed::Session> session) {
    close_session(session, restbed::OK, nlohmann::json(current_session.calculate_aligned_tubes()));
}

// @formatter:off
void restfull_api_create_endpoints(restbed::Service &service) {
    std::map<std::string, std::vector<ResourceEntry>> rest_resources = {
        {"REMA/connect", {{"POST", &REMA_connect}}},
        {"REMA/info", {{"GET", &REMA_info}}},
        {"HXs", {{"GET", &HXs_list_}}},
        {"HXs/tubesheet/load", {{"GET", &HXs_tubesheet_load}}},
        {"inspection-plans", {{"GET", &inspection_plans}}},
        {"inspection-plans/{insp_plan: .*}", {{"GET", &inspection_plans}}},
        {"tools", {{"GET", &tools_list},
                   {"POST", &tools_create},
                  }
        },
        {"tools/{tool_name: .*}", {{"DELETE", &tools_delete}}},
        {"tools/{tool_name: .*}/select", {{"POST", &tools_select}}},
        {"inspection-sessions", {{"GET", &inspection_sessions_list},
                                 {"POST", &inspection_sessions_create},
                                },
        },
        {"inspection-sessions/{session_name: .*}", {{"GET", &inspection_sessions_load},
                                                    {"DELETE", &inspection_sessions_delete}
                                                   }
        },
        {"current-session/info", {{"GET", &inspection_sessions_info}}},
        {"calibration-points", {{"GET", &cal_points_list},
                                {"POST", &cal_points_add},
                               },
        },
        {"calibration-points/{tube_id: .*}", {{"PUT", &cal_points_update},
                                              {"DELETE", &cal_points_delete}}
                                             },
        {"tubes/{tube_id: .*}", {{"PUT", &tubes_set_status}}},
        {"determine-tube-center/{tube_id: .*}", {{"GET", &determine_tube_center}}},
        {"aligned-tubesheet-get", {{"GET", &aligned_tubesheet_get}}},
    };
// @formatter:on

    //std::cout << "Creando endpoints" << "\n";
    for (auto [path, resources] : rest_resources) {
        auto resource_rest = std::make_shared<restbed::Resource>();
        resource_rest->set_path(std::string("/REST/").append(path));
        //resource_rest->set_failed_filter_validation_handler(
        //        failed_filter_validation_handler);

        for (ResourceEntry r : resources) {
            resource_rest->set_method_handler(r.method, r.callback);
            //std::cout << "127.0.0.1:4321/REST/" << path << ", " << r.method << "\n";
        }
        service.publish(resource_rest);
    }
}
