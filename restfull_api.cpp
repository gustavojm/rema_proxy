#include <functional>
#include <ctime>
#include <thread>
#include <chrono>
#include <fstream>
#include <iostream>
#include <exception>
#include <vector>
#include <memory>
#include <cstdint>

#include "json.hpp"
#include "inspection-session.hpp"
#include "tool.hpp"
#include "rema.hpp"
#include "HXs.hpp"
#include "points.hpp"
#include "circle_fns.hpp"
#include "misc_fns.hpp"


extern InspectionSession current_session;

void close_session(const std::shared_ptr<restbed::Session>& session, int status) {
    session->close(status, "", { { "Content-Type", "text/html ; charset=utf-8" }, { "Content-Length", "0"} });
}

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

    res["tools"] = REMA::tools;
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
    close_session(session, restbed::OK,  nlohmann::json(current_session.tubes));
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
    REMA &rema_instance = REMA::get_instance();
    close_session(session, restbed::OK, rema_instance.tools);
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
                        double offset_x = to_double(form_data.value("offset_x", "0"));
                        double offset_y = to_double(form_data.value("offset_y", "0"));
                        double offset_z = to_double(form_data.value("offset_z", "0"));

                        Tool new_tool(tool_name, {offset_x, offset_y, offset_z});
                        REMA::add_tool(new_tool);
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
    rema_instance.set_last_selected_tool(tool_name);
    close_session(session, restbed::OK);
}


/**
 * Inspection Sessions related functions
 **/

void inspection_sessions_list(const std::shared_ptr<restbed::Session> session) {
    nlohmann::json res;

    for (auto session : InspectionSession::sessions_list()) {
        res.push_back({
            {"name", session.name},
            {"hx", session.hx },
            {"last_write_time", session.last_write_time},
            {"total_tubes_in_plans", session.total_tubes_in_plans},
            {"total_tubes_inspected", session.total_tubes_inspected}
        });
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
            current_session.process_csv();
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
        res["tubes"] = current_session.tubes;
        res["aligned_tubes"] = current_session.aligned_tubes;
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

void cal_points_add_update(const std::shared_ptr<restbed::Session> session) {
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
                    Point3D ideal_coords = {
                            to_double(form_data.value("ideal_coords_x", "0")),
                            to_double(form_data.value("ideal_coords_y", "0")),
                            to_double(form_data.value("ideal_coords_z", "0")),
                    };

                    Point3D determined_coords = {
                            to_double(form_data.value("determined_coords_x", "0")),
                            to_double(form_data.value("determined_coords_y", "0")),
                            to_double(form_data.value("determined_coords_z", "0")),
                    };
                    if (!tube_id.empty()) {
                        current_session.cal_points_add_update(tube_id, form_data["col"], form_data["row"], ideal_coords, determined_coords);
                        status = restbed::OK;
                    } else {
                        res += "No tube specified";
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

void axes_hard_stop_all(const std::shared_ptr<restbed::Session> session) {
    REMA &rema_instance = REMA::get_instance();
    rema_instance.axes_hard_stop_all();
    close_session(session, restbed::OK);
}

void axes_soft_stop_all(const std::shared_ptr<restbed::Session> session) {
    std::cout << "received soft stop \n";
    REMA &rema_instance = REMA::get_instance();
    rema_instance.axes_soft_stop_all();
    close_session(session, restbed::OK);
}

void go_to_tube(const std::shared_ptr<restbed::Session> session) {
    REMA &rema_instance = REMA::get_instance();
    const auto request = session->get_request();
    std::string tube_id = request->get_path_parameter("tube_id", "");

    nlohmann::json res;

    if (!tube_id.empty()) {
        Tool t = rema_instance.get_selected_tool();
        Point3D rema_coords = current_session.from_ui_to_rema(current_session.get_tube_coordinates(tube_id, false), &t);

        movement_cmd goto_tube;
        goto_tube.axes = "XY";
        goto_tube.first_axis_setpoint = rema_coords.x;
        goto_tube.second_axis_setpoint = rema_coords.y;
        goto_tube.stop_on_probe = true;
        goto_tube.stop_on_condition = true;

        rema_instance.move_closed_loop(goto_tube);

        close_session(session, restbed::OK, res);
    }
}

void move_free_run(const std::shared_ptr<restbed::Session> session) {
    REMA &rema_instance = REMA::get_instance();
    const auto request = session->get_request();
    std::string dir = request->get_path_parameter("dir", "");

    constexpr int positive_big_number = INT32_MAX - 100000;     // Consider that already_there() compares with MOT_PAP_POS_THRESHOLD;
    constexpr int negative_big_number = INT32_MIN + 100000;     //

    nlohmann::json pars_obj;

    if (dir.find("left") != std::string::npos) {
        pars_obj["axes"] = "XY";
        pars_obj["first_axis_setpoint"] = negative_big_number;
    }

    if (dir.find("right") != std::string::npos) {
        pars_obj["axes"] = "XY";
        pars_obj["first_axis_setpoint"] = positive_big_number;
    }

    if (dir.find("up") != std::string::npos) {
        pars_obj["axes"] = "XY";
        pars_obj["second_axis_setpoint"] = positive_big_number;
    }

    if (dir.find("down") != std::string::npos) {
        pars_obj["axes"] = "XY";
        pars_obj["second_axis_setpoint"] = negative_big_number;
    }

    if (dir.find("z_in") != std::string::npos) {
        pars_obj["axes"] = "Z";
        pars_obj["first_axis_setpoint"] = positive_big_number;
    }

    if (dir.find("z_out") != std::string::npos) {
        pars_obj["axes"] = "Z";
        pars_obj["first_axis_setpoint"] = negative_big_number;
    }

    rema_instance.axes_soft_stop_all();
    rema_instance.execute_command({{"command", "MOVE_FREE_RUN"}, {"pars", pars_obj}});
    close_session(session, restbed::OK);
}

bool approximatelly_equal(double f1, double f2) {
    return (fabs(f1 - f2) < 0.001d);
}

void move_incremental(const std::shared_ptr<restbed::Session> session) {
    REMA &rema_instance = REMA::get_instance();
    nlohmann::json pars_obj;
    const auto request = session->get_request();
    size_t content_length = request->get_header("Content-Length", 0);
    session->fetch(content_length,
            [&](const std::shared_ptr<restbed::Session> &session,
                    const restbed::Bytes &body) {
        nlohmann::json form_data = nlohmann::json::parse(body.begin(), body.end());

        double incremental_x = 0.d;
        double incremental_y = 0.d;
        double incremental_z = 0.d;

        if (form_data.contains("incremental_x")) {
            incremental_x = to_double(form_data["incremental_x"]);
            if (!approximatelly_equal(incremental_x, 0)) {
                pars_obj["axes"] = "XY";
                pars_obj["first_axis_delta"] = current_session.from_ui_to_rema(incremental_x);
            }
        }

        if (form_data.contains("incremental_y")) {
            incremental_y = to_double(form_data["incremental_y"]);
            if (!approximatelly_equal(incremental_y, 0)) {
                pars_obj["axes"] = "XY";
                pars_obj["second_axis_delta"] = current_session.from_ui_to_rema(incremental_y);
            }
        }

        if (form_data.contains("incremental_z")) {
            incremental_z = to_double(form_data["incremental_z"]);
            if (!approximatelly_equal(incremental_z, 0)) {
                pars_obj["axes"] = "Z";
                pars_obj["first_axis_delta"] = current_session.from_ui_to_rema(incremental_z);
            }
        }
        rema_instance.axes_soft_stop_all();
        rema_instance.execute_command({{"command", "MOVE_INCREMENTAL"}, {"pars", pars_obj}});
        close_session(session, restbed::OK);
    });
}


void set_home_xy(const std::shared_ptr<restbed::Session> session) {
    REMA &rema_instance = REMA::get_instance();
    Tool t = rema_instance.get_selected_tool();

    const auto request = session->get_request();
    std::string tube_id = request->get_path_parameter("tube_id", "");
    if (!tube_id.empty()) {
        Point3D tube_coords = current_session.from_ui_to_rema(current_session.get_tube_coordinates(tube_id, true), &t);
        rema_instance.set_home_xy(tube_coords.x, tube_coords.y);
    } else {
        Point3D zero_coords = current_session.from_ui_to_rema(Point3D() , &t);
        rema_instance.set_home_xy(zero_coords.x, zero_coords.y);
    }
    close_session(session, restbed::OK);
}

void set_home_z(const std::shared_ptr<restbed::Session> session) {
    REMA &rema_instance = REMA::get_instance();
    Tool t = rema_instance.get_selected_tool();

    const auto request = session->get_request();
    double z = request->get_path_parameter("z", 0.d);

    nlohmann::json res;

    double corrected_z = current_session.from_ui_to_rema(z) + t.offset.z;
    rema_instance.set_home_z(corrected_z);
    close_session(session, restbed::OK, res);
}


void determine_tube_center(const std::shared_ptr<restbed::Session> session) {
    REMA &rema_instance = REMA::get_instance();
    Tool t = rema_instance.get_selected_tool();
    const auto request = session->get_request();
    std::string tube_id = request->get_path_parameter("tube_id", "");

    nlohmann::json res;

    if (!tube_id.empty()) {
        Tool t = rema_instance.get_selected_tool();
        double tube_radius = current_session.from_ui_to_rema(current_session.tube_od / 2);

        constexpr int points_number = 5;
        static_assert(points_number % 2 != 0, "Number of points must be odd");
        std::vector<Point3D> reordered_points;
        std::vector<Point3D> points = calculateCirclePoints(current_session.from_ui_to_rema(current_session.get_tube_coordinates(tube_id, true), &t), tube_radius, points_number);

        int i = 0;
        for (int n = 0; n < points_number; n++) {
            reordered_points.push_back(points[i % points_number]);
            i += 2;
        }

        std::vector<movement_cmd> seq;
        for (auto point : reordered_points) {
            movement_cmd step;
            step.axes = "XY";
            step.first_axis_setpoint = point.x;
            step.second_axis_setpoint = point.y;
            step.stop_on_probe = true;
            step.stop_on_condition = true;

            seq.push_back(step);
        }

        if (!rema_instance.execute_sequence(seq)) {
            close_session(session, restbed::RESET_CONTENT);
            return;
        }

        std::vector<Point3D> tube_boundary_points;

        for (auto step : seq) {
            if (step.executed && (step.execution_results.stopped_on_probe || step.execution_results.stopped_on_condition)) {
                tube_boundary_points.push_back(step.execution_results.coords);
            }
        }
        res["reached_coords"] = tube_boundary_points;
        Circle c = CircleFitByHyper(tube_boundary_points);
        res["center"] = {
                {"x", c.center.x - t.offset.x},
                {"y", c.center.y - t.offset.y},
                {"z", c.center.z}
        };
        res["radius"] = c.radius;

        movement_cmd goto_center;
        goto_center.axes = "XY";
        goto_center.first_axis_setpoint = c.center.x;
        goto_center.second_axis_setpoint = c.center.y;
        goto_center.stop_on_probe = true;
        goto_center.stop_on_condition = true;

        rema_instance.move_closed_loop(goto_center);

        close_session(session, restbed::OK, res);
    }
}

void determine_tubesheet_z(const std::shared_ptr<restbed::Session> session) {
    REMA &rema_instance = REMA::get_instance();

    nlohmann::json res;
    std::vector<movement_cmd> seq;
    movement_cmd forewards;
    forewards.axes = "Z";
    forewards.first_axis_setpoint = 0.2;
    forewards.second_axis_setpoint = 0;
    forewards.stop_on_probe = true;
    forewards.stop_on_condition = true;

    movement_cmd backwards = forewards;
    backwards.first_axis_setpoint = -0.2;

    seq.push_back(forewards);
    seq.push_back(backwards);
    seq.push_back(forewards);
    seq.push_back(backwards);

    rema_instance.execute_sequence(seq);

    double sum_z = 0;
    int count = 0;
    for (auto step : seq) {
        if (step.executed && step.execution_results.stopped_on_condition) {  // average all the touches (ask for stopped_on_probe
            sum_z += step.execution_results.coords.z;
            count++;
        }
    }

    double z = sum_z / count;

    movement_cmd goto_tubeshet;
    goto_tubeshet.axes = "Z";
    goto_tubeshet.first_axis_setpoint = z;
    goto_tubeshet.second_axis_setpoint = 0;
    goto_tubeshet.stop_on_probe = true;                                // to reach the averaged z it should not stop on probe...
    goto_tubeshet.stop_on_condition = true;

    rema_instance.move_closed_loop(goto_tubeshet);

    close_session(session, restbed::OK, res);

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
        {"calibration-points", {{"GET", &cal_points_list}}},
        {"calibration-points/{tube_id: .*}", {{"PUT", &cal_points_add_update},
                                              {"DELETE", &cal_points_delete}}
                                             },
        {"tubes/{tube_id: .*}", {{"PUT", &tubes_set_status}}},
        {"go-to-tube/{tube_id: .*}", {{"GET", &go_to_tube}}},
        {"move-free-run/{dir: .*}", {{"GET", &move_free_run}}},
        {"move-incremental", {{"POST", &move_incremental}}},
        {"determine-tube-center/{tube_id: .*}", {{"GET", &determine_tube_center}}},
        {"set-home-xy/", {{"GET", &set_home_xy}}},
        {"set-home-xy/{tube_id: .*}", {{"GET", &set_home_xy}}},
        {"determine-tubesheet-z", {{"GET", &determine_tubesheet_z}}},
        {"set-home-z/{z: .*}", {{"GET", &set_home_z}}},
        {"aligned-tubesheet-get", {{"GET", &aligned_tubesheet_get}}},
        {"axes-hard-stop-all", {{"GET", &axes_hard_stop_all}}},
        {"axes-soft-stop-all", {{"GET", &axes_soft_stop_all}}},

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
