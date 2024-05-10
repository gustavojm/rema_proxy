#include <chrono>
#include <cstdint>
#include <ctime>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <spdlog/spdlog.h>
#include <thread>
#include <vector>

#include "HX.hpp"
#include "circle_fns.hpp"
#include "json.hpp"
#include "magic_enum.hpp"
#include "misc_fns.hpp"
#include "points.hpp"
#include "rema.hpp"
#include "session.hpp"
#include "tool.hpp"

extern Session current_session;

void close_rest_session(const std::shared_ptr<restbed::Session> &rest_session, int status) {
    rest_session->close(status, "", { { "Content-Type", "text/html ; charset=utf-8" }, { "Content-Length", "0" } });
}

void close_rest_session(const std::shared_ptr<restbed::Session> &rest_session, int status, const std::string &res) {
    rest_session->close(
        status,
        res,
        { { "Content-Type", "text/html ; charset=utf-8" }, { "Content-Length", std::to_string(res.length()) } });
}

void close_rest_session(const std::shared_ptr<restbed::Session> &rest_session, int status, const nlohmann::json &json_res) {
    std::string res = json_res.dump();
    rest_session->close(
        status,
        res,
        { { "Content-Type", "application/json ; charset=utf-8" }, { "Content-Length", std::to_string(res.length()) } });
}

struct ResourceEntry {
    std::string method;
    std::function<void(const std::shared_ptr<restbed::Session>)> callback;
};

/**
 * REMA related functions
 **/
void REMA_connect(const std::shared_ptr<restbed::Session> &rest_session) {
    std::string res;
    int status = restbed::OK;
    try {
        REMA &rema_instance = REMA::get_instance();
        rema_instance.command_client.connect();
        rema_instance.telemetry_client.connect();
        status = restbed::OK;
    } catch (std::exception &e) {
        res = e.what();
        status = restbed::INTERNAL_SERVER_ERROR;
    }
    close_rest_session(rest_session, status, res);
}

void REMA_info(const std::shared_ptr<restbed::Session> &rest_session) {
    nlohmann::json res = nlohmann::json(nlohmann::json::value_t::object);
    REMA &rema_instance = REMA::get_instance();

    std::map<std::string, Tool> tools_to_ui;
    for (auto [id, tool] : REMA::tools) {
        tools_to_ui[id] = Tool(id, (tool.offset * current_session.hx.scale), tool.is_touch_probe);
    }

    res["tools"] = tools_to_ui;
    res["last_selected_tool"] = rema_instance.last_selected_tool;
    res["host"] = rema_instance.command_client.get_host();
    res["service"] = rema_instance.command_client.get_service();
    close_rest_session(rest_session, restbed::OK, res);
}

/**
 * HX related functions
 **/
void HXs_list(const std::shared_ptr<restbed::Session> &rest_session) {
    close_rest_session(rest_session, restbed::OK, nlohmann::json(HX::list()));
}

void HXs_delete(const std::shared_ptr<restbed::Session> &rest_session) {
    const auto request = rest_session->get_request();
    std::string HX_name = request->get_path_parameter("HX_name", "");

    nlohmann::json res;
    if (!HX_name.empty()) {
        if (!HX::erase(HX_name)) {
            res["error"] = "Unable to delete Heat Exchanger";
        }
    }
    close_rest_session(rest_session, restbed::OK);
}

void HXs_tubesheet_load(const std::shared_ptr<restbed::Session> &rest_session) {
    close_rest_session(rest_session, restbed::OK, nlohmann::json(current_session.hx.tubes));
}

/**
 * Plans related functions
 **/

void plans(const std::shared_ptr<restbed::Session> &rest_session) {
    const auto request = rest_session->get_request();
    std::string plan = request->get_path_parameter("plan", "");

    nlohmann::json res;
    if (!plan.empty()) {
        res = current_session.plan_get(plan);
    }
    close_rest_session(rest_session, restbed::OK, res);
}

void plans_delete(const std::shared_ptr<restbed::Session> &rest_session) {
    const auto request = rest_session->get_request();
    std::string plan = request->get_path_parameter("plan", "");

    nlohmann::json res;
    if (!plan.empty()) {
        current_session.plan_remove(plan);
    }
    close_rest_session(rest_session, restbed::OK);
}

/**
 * Tools related functions
 **/

void tools_list(const std::shared_ptr<restbed::Session> &rest_session) {
    close_rest_session(rest_session, restbed::OK, REMA::tools);
}

void tools_create(const std::shared_ptr<restbed::Session> rest_session) {
    const auto request = rest_session->get_request();
    size_t content_length = request->get_header("Content-Length", 0);
    rest_session->fetch(
        content_length,
        [&]([[maybe_unused]] const std::shared_ptr<restbed::Session> &rest_session_ptr, const restbed::Bytes &body) {
            nlohmann::json form_data = nlohmann::json::parse(body.begin(), body.end());
            std::string res;
            int status = restbed::OK;

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
                    bool is_touch_probe = form_data.value("is_touch_probe", "off") == "on";
                    Tool new_tool(tool_name, { offset_x, offset_y, offset_z }, is_touch_probe);
                    new_tool.save_to_disk();
                    REMA::add_tool(new_tool);
                    res = "Tool created Successfully";
                    status = restbed::CREATED;
                }
            } catch (const std::invalid_argument &e) {
                res += "Offsets wrong";
                status = restbed::BAD_REQUEST;
            } catch (const std::exception &e) {
                res = e.what();
                status = restbed::INTERNAL_SERVER_ERROR;
            }
            close_rest_session(rest_session_ptr, status, res);
        });
}

void tools_delete(const std::shared_ptr<restbed::Session> &rest_session) {
    const auto request = rest_session->get_request();
    std::string tool_name = request->get_path_parameter("tool_name", "");

    std::string res;
    int status = restbed::OK;
    try {
        REMA::delete_tool(tool_name);
        status = restbed::NO_CONTENT;
    } catch (const std::filesystem::filesystem_error &e) {
        res = "Failed to delete the tool";
        status = restbed::INTERNAL_SERVER_ERROR;
    }
    close_rest_session(rest_session, status, res);
}

void tools_select(const std::shared_ptr<restbed::Session> &rest_session) {
    auto request = rest_session->get_request();
    std::string tool_name = request->get_path_parameter("tool_name", "");
    REMA &rema_instance = REMA::get_instance();
    rema_instance.set_last_selected_tool(tool_name);
    close_rest_session(rest_session, restbed::OK);
}

/**
 * Sessions related functions
 **/

void sessions_list(const std::shared_ptr<restbed::Session> &rest_session) {
    nlohmann::json res;

    for (auto &session : Session::sessions_list()) {
        res.push_back({ { "name", session.name },
                        { "hx", session.hx },
                        { "hx_dir", session.hx_dir },
                        { "last_write_time", session.last_write_time },
                        { "total_tubes_in_plans", session.total_tubes_in_plans() },
                        { "total_tubes_executed", session.total_tubes_executed() } });
    }
    close_rest_session(rest_session, restbed::OK, res);
}

void sessions_create(const std::shared_ptr<restbed::Session> &rest_session) {
    const auto request = rest_session->get_request();
    size_t content_length = request->get_header("Content-Length", 0);
    rest_session->fetch(
        content_length,
        [&]([[maybe_unused]] const std::shared_ptr<restbed::Session> &rest_session_ptr, const restbed::Bytes &body) {
            nlohmann::json form_data = nlohmann::json::parse(body.begin(), body.end());
            std::string res;
            int status = restbed::OK;
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
                    Session new_session(session_name, std::filesystem::path(form_data["hx"]));
                    res = new_session.load_plans();
                    new_session.save_to_disk();
                    current_session = new_session;
                    status = restbed::CREATED;
                }
            } catch (const std::exception &e) {
                res += "Error creating Session \n";
                status = restbed::INTERNAL_SERVER_ERROR;
            }
            close_rest_session(rest_session_ptr, status, res);
        });
}

void sessions_load(const std::shared_ptr<restbed::Session> &rest_session) {
    auto request = rest_session->get_request();
    std::string session_name = request->get_path_parameter("session_name", "");
    int status = restbed::OK;
    std::string res;
    if (!session_name.empty()) {
        try {
            current_session.load(session_name);
            current_session.hx.process_csv_from_disk(current_session.hx_dir);
            current_session.hx.generate_svg();
            current_session.copy_tubes_to_aligned_tubes();
            current_session.calculate_aligned_tubes();

            status = restbed::OK;
        } catch (std::exception &e) {
            res = e.what();
            status = restbed::INTERNAL_SERVER_ERROR;
        }
    } else {
        res = "No session selected";
        status = restbed::INTERNAL_SERVER_ERROR;
    }
    close_rest_session(rest_session, status, res);
}

void sessions_info(const std::shared_ptr<restbed::Session> &rest_session) {
    nlohmann::json res = nlohmann::json::object();
    if (current_session.is_loaded()) {
        res = current_session;
        res["tubes"] = current_session.hx.tubes;
        res["aligned_tubes"] = current_session.aligned_tubes;
        res["is_loaded"] = true;
    } else {
        res["is_loaded"] = false;
    }
    close_rest_session(rest_session, restbed::OK, res);
}

void sessions_delete(const std::shared_ptr<restbed::Session> &rest_session) {
    auto request = rest_session->get_request();
    std::string session_name = request->get_path_parameter("session_name", "");
    try {
        Session::delete_session(session_name);
        close_rest_session(rest_session, restbed::OK, nlohmann::json(current_session));
        return;
    } catch (const std::filesystem::filesystem_error &e) {
        std::string res = std::string("filesystem error: ") + e.what();
        close_rest_session(rest_session, restbed::INTERNAL_SERVER_ERROR, res);
    }
}

/**
 * Calibration Points related functions
 **/

void cal_points_list(const std::shared_ptr<restbed::Session> &rest_session) {
    close_rest_session(rest_session, restbed::OK, nlohmann::json(current_session.cal_points));
}

void cal_points_add_update(const std::shared_ptr<restbed::Session> rest_session) {
    const auto request = rest_session->get_request();
    std::string tube_id = request->get_path_parameter("tube_id", "");

    size_t content_length = request->get_header("Content-Length", 0);
    rest_session->fetch(
        content_length,
        [&]([[maybe_unused]] const std::shared_ptr<restbed::Session> &rest_session_ptr, const restbed::Bytes &body) {
            nlohmann::json form_data = nlohmann::json::parse(body.begin(), body.end());
            std::string res;
            int status = restbed::OK;

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
                    current_session.cal_points_add_update(
                        tube_id, form_data["col"], form_data["row"], ideal_coords, determined_coords);
                    status = restbed::OK;
                } else {
                    res += "No tube specified";
                    status = restbed::BAD_REQUEST;
                }
            } catch (std::exception &e) {
                res += e.what();
                status = restbed::INTERNAL_SERVER_ERROR;
            }
            close_rest_session(rest_session_ptr, status, res);
        });
};

void cal_points_delete(const std::shared_ptr<restbed::Session> &rest_session) {
    const auto request = rest_session->get_request();
    std::string tube_id = request->get_path_parameter("tube_id", "");

    std::string res;
    int status = restbed::OK;
    if (tube_id.empty()) {
        res = "No tube specified";
        status = restbed::INTERNAL_SERVER_ERROR;
    } else {
        current_session.cal_points_delete(tube_id);
        status = restbed::NO_CONTENT;
    }
    close_rest_session(rest_session, status, res);
}

void tubes_set_status(const std::shared_ptr<restbed::Session> rest_session) {

    const auto request = rest_session->get_request();
    std::string tube_id = request->get_path_parameter("tube_id", "");

    size_t content_length = request->get_header("Content-Length", 0);
    std::string session_name = request->get_path_parameter("session_name", "");

    rest_session->fetch(
        content_length,
        [&]([[maybe_unused]] const std::shared_ptr<restbed::Session> &rest_session_ptr, const restbed::Bytes &body) {
            nlohmann::json form_data = nlohmann::json::parse(body.begin(), body.end());
            std::string res_string;

            std::string plan = form_data["plan"];
            bool checked = form_data["checked"];
            current_session.set_tube_executed(plan, tube_id, checked);
            nlohmann::json res = nlohmann::json::object();
            res[tube_id] = checked;
            close_rest_session(rest_session_ptr, restbed::OK, res);
        });
}

void axes_hard_stop_all(const std::shared_ptr<restbed::Session> &rest_session) {
    REMA &rema_instance = REMA::get_instance();
    rema_instance.axes_hard_stop_all();
    close_rest_session(rest_session, restbed::OK);
}

void axes_soft_stop_all(const std::shared_ptr<restbed::Session> &rest_session) {
    SPDLOG_INFO("Received soft stop");
    REMA &rema_instance = REMA::get_instance();
    rema_instance.axes_soft_stop_all();
    close_rest_session(rest_session, restbed::OK);
}

void go_to_tube(const std::shared_ptr<restbed::Session> &rest_session) {
    REMA &rema_instance = REMA::get_instance();
    const auto request = rest_session->get_request();
    std::string tube_id = request->get_path_parameter("tube_id", "");

    nlohmann::json res;

    if (!tube_id.empty()) {
        Tool tool = rema_instance.get_selected_tool();
        Point3D rema_coords = current_session.from_ui_to_rema(current_session.get_tube_coordinates(tube_id, false), &tool);

        movement_cmd goto_tube;
        goto_tube.axes = "XY";
        goto_tube.first_axis_setpoint = rema_coords.x;
        goto_tube.second_axis_setpoint = rema_coords.y;
        rema_instance.move_closed_loop(goto_tube);

        close_rest_session(rest_session, restbed::OK, res);
    }
}

void move_free_run(const std::shared_ptr<restbed::Session> &rest_session) {
    REMA &rema_instance = REMA::get_instance();
    const auto request = rest_session->get_request();
    std::string dir = request->get_path_parameter("dir", "");

    constexpr int positive_big_number =
        INT32_MAX / 2; // This value goes into bresenham error determination that needs to be multiplied by 2
    constexpr int negative_big_number = INT32_MIN / 2;

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
    rema_instance.execute_command({ { "command", "MOVE_FREE_RUN" }, { "pars", pars_obj } });
    close_rest_session(rest_session, restbed::OK);
}

bool equals(double f1, double f2) {
    return (fabs(f1 - f2) < 0.000001); /* EPSILON */
}

void change_network_settings(const std::shared_ptr<restbed::Session> &rest_session) {
    REMA &rema_instance = REMA::get_instance();
    nlohmann::json pars_obj;
    const auto request = rest_session->get_request();
    size_t content_length = request->get_header("Content-Length", 0);
    rest_session->fetch(
        content_length,
        [&]([[maybe_unused]] const std::shared_ptr<restbed::Session> &rest_session_ptr, const restbed::Bytes &body) {
            nlohmann::json form_data = nlohmann::json::parse(body.begin(), body.end());

            if (isValidIPv4(form_data["ipaddr"])) {
                std::string rtu_host = form_data["ipaddr"];
                std::string rtu_port = form_data["port"];

                rema_instance.config["REMA"]["network"]["ip"] = rtu_host;
                rema_instance.config["REMA"]["network"]["port"] = std::stoi(rtu_port);
                rema_instance.save_config();

                if (form_data.contains("change_remote_network_settings")) {
                    pars_obj["ipaddr"] = rtu_host;
                    pars_obj["port"] = std::stoi(rtu_port);
                    pars_obj["gw"] = form_data["ipaddr"];
                    pars_obj["netmask"] = "255.255.255.0";
                    rema_instance.execute_command_no_wait({ { "command", "NETWORK_SETTINGS" }, { "pars", pars_obj } });
                }

                for (int retry = 0; retry < 3; ++retry) {
                    try {
                        rema_instance.connect(rtu_host, rtu_port);
                    } catch (const std::exception &e) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        SPDLOG_INFO("Retrying...");
                    }
                }
            }
            close_rest_session(rest_session_ptr, restbed::OK);
        });
}

void move_incremental(const std::shared_ptr<restbed::Session> &rest_session) {
    REMA &rema_instance = REMA::get_instance();
    const auto request = rest_session->get_request();
    size_t content_length = request->get_header("Content-Length", 0);
    rest_session->fetch(
        content_length,
        [&]([[maybe_unused]] const std::shared_ptr<restbed::Session> &rest_session_ptr, const restbed::Bytes &body) {
            std::string s(body.begin(), body.end());
            nlohmann::json pars_obj;
            nlohmann::json form_data = nlohmann::json::parse(body.begin(), body.end());
            double incremental_x = 0.0;
            double incremental_y = 0.0;
            double incremental_z = 0.0;
            if (form_data.contains("incremental_x")) {
                incremental_x = to_double(form_data["incremental_x"]);
                if (!equals(incremental_x, 0)) {
                    pars_obj["axes"] = "XY";
                    pars_obj["first_axis_delta"] = current_session.from_ui_to_rema(incremental_x);
                }
            }
            if (form_data.contains("incremental_y")) {
                incremental_y = to_double(form_data["incremental_y"]);
                if (!equals(incremental_y, 0)) {
                    pars_obj["axes"] = "XY";
                    pars_obj["second_axis_delta"] = current_session.from_ui_to_rema(incremental_y);
                }
            }
            if (form_data.contains("incremental_z")) {
                incremental_z = to_double(form_data["incremental_z"]);
                if (!equals(incremental_z, 0)) {
                    pars_obj["axes"] = "Z";
                    pars_obj["first_axis_delta"] = current_session.from_ui_to_rema(incremental_z);
                }
            }
            rema_instance.axes_soft_stop_all();
            rema_instance.execute_command({ { "command", "MOVE_INCREMENTAL" }, { "pars", pars_obj } });
            close_rest_session(rest_session_ptr, restbed::OK);
        });
}

void set_home_xy(const std::shared_ptr<restbed::Session> &rest_session) {
    REMA &rema_instance = REMA::get_instance();
    Tool tool = rema_instance.get_selected_tool();

    const auto request = rest_session->get_request();
    std::string tube_id = request->get_path_parameter("tube_id", "");
    if (!tube_id.empty()) {
        Point3D tube_coords = current_session.from_ui_to_rema(current_session.get_tube_coordinates(tube_id, true), &tool);
        rema_instance.set_home_xy(tube_coords.x, tube_coords.y);
    } else {
        Point3D zero_coords = current_session.from_ui_to_rema(Point3D(), &tool);
        rema_instance.set_home_xy(zero_coords.x, zero_coords.y);
    }
    close_rest_session(rest_session, restbed::OK);
}

void set_home_z(const std::shared_ptr<restbed::Session> &rest_session) {
    REMA &rema_instance = REMA::get_instance();
    Tool tool = rema_instance.get_selected_tool();

    const auto request = rest_session->get_request();
    double z = request->get_path_parameter("z", 0.0);

    nlohmann::json res;

    double corrected_z = current_session.from_ui_to_rema(z) + tool.offset.z;
    rema_instance.set_home_z(corrected_z);
    close_rest_session(rest_session, restbed::OK, res);
}

void determine_tube_center(const std::shared_ptr<restbed::Session> &rest_session) {
    REMA &rema_instance = REMA::get_instance();
    Tool tool = rema_instance.get_selected_tool();
    double probe_wiggle_factor = 1.2;
    double half_probe_wiggle_factor = 1 + ((probe_wiggle_factor - 1) / 2);
    const auto request = rest_session->get_request();
    std::string tube_id = request->get_path_parameter("tube_id", "");
    bool set_home = request->get_path_parameter("set_home", "") == "true";

    nlohmann::json res;

    if (!tube_id.empty()) {
        double tube_radius = current_session.hx.tube_od / 2;
        Point3D ideal_center = current_session.get_tube_coordinates(tube_id, true);

        if (auto distance =
                std::abs(rema_instance.telemetry.coords.distance_xy(current_session.from_ui_to_rema(ideal_center, &tool)));
            distance > tube_radius * probe_wiggle_factor) {
            res["error"] = "Please have the touch probe inserted into the tube to be detected";
            close_rest_session(rest_session, restbed::CONFLICT, res);
            return;
        }

        constexpr int points_number = 5;
        static_assert(points_number % 2 != 0, "Number of points must be odd");
        std::vector<Point3D> reordered_points;
        std::vector<Point3D> points = calculateCirclePoints(
            current_session.from_ui_to_rema(ideal_center, &tool),
            current_session.from_ui_to_rema(tube_radius) * half_probe_wiggle_factor,
            points_number);

        int vertex = 0;
        for (int n = 0; n < points_number; n++) {
            reordered_points.push_back(
                points[vertex % points_number]); // To touch the tube boundary following a star pattern
            vertex += 2;
        }

        std::vector<movement_cmd> seq;
        for (auto &point : reordered_points) {
            movement_cmd step;
            step.axes = "XY";
            step.first_axis_setpoint = point.x;
            step.second_axis_setpoint = point.y;
            seq.push_back(step);
        }

        if (!rema_instance.execute_sequence(seq)) {
            res["error"] = "Executing sequence";
            close_rest_session(rest_session, restbed::RESET_CONTENT, res);
            return;
        }

        std::vector<Point3D> tube_boundary_points;

        for (const auto &step : seq) {
            if (step.executed && (step.execution_results.stopped_on_probe || step.execution_results.stopped_on_condition)) {
                tube_boundary_points.push_back(step.execution_results.coords);
            }
        }

        int status = restbed::OK;
        Circle circle = CircleFitByHyper(tube_boundary_points);

        if (auto distance = std::abs(rema_instance.telemetry.coords.distance_xy(circle.center));
            distance > tube_radius * probe_wiggle_factor) {
            res["error"] = fmt::format(
                "Point determined too far away, would break probe x: {}, y: {}", circle.center.x, circle.center.y);
            status = restbed::CONFLICT;
        } else {
            std::vector<movement_cmd> goto_center_seq;
            movement_cmd goto_center;
            goto_center.axes = "XY";
            goto_center.first_axis_setpoint = circle.center.x;
            goto_center.second_axis_setpoint = circle.center.y;
            goto_center_seq.push_back(goto_center);

            res["center"] = { { "x", circle.center.x - tool.offset.x },
                              { "y", circle.center.y - tool.offset.y },
                              { "z", circle.center.z } };
            res["radius"] = circle.radius;

            if (!rema_instance.execute_sequence(goto_center_seq)) {
                res["error"] = "Executing sequence";
                status = restbed::CONFLICT;
                return;
            } else if (set_home) {
                auto step = goto_center_seq.begin();
                if (step->executed && step->execution_results.stopped_on_condition) {
                    rema_instance.set_home_xy(
                        current_session.from_ui_to_rema(ideal_center.x) + tool.offset.x,
                        current_session.from_ui_to_rema(ideal_center.y) + tool.offset.y);
                }
            }
        }

        close_rest_session(rest_session, status, res);
    }
}

void determine_tubesheet_z(const std::shared_ptr<restbed::Session> &rest_session) {
    const auto request = rest_session->get_request();
    REMA &rema_instance = REMA::get_instance();

    nlohmann::json res;
    std::vector<movement_cmd> seq;
    movement_cmd forewards;
    forewards.axes = "Z";
    forewards.first_axis_setpoint = 0.5;
    forewards.second_axis_setpoint = 0;

    movement_cmd backwards = forewards;
    backwards.first_axis_setpoint = -0.5;

    seq.push_back(forewards);
    seq.push_back(backwards);
    seq.push_back(forewards);
    seq.push_back(backwards);

    rema_instance.execute_sequence(seq);

    double sum_z = 0;
    int count = 0;
    for (const auto &step : seq) {
        if (step.executed && step.execution_results.stopped_on_probe) {
            sum_z += step.execution_results.coords.z;
            count++;
        }
    }

    int status = restbed::OK;

    int required_touches = 2;
    if (count < required_touches) {
        status = restbed::CONFLICT;
        res["error"] = fmt::format(
            "Probe didn't touch the tubesheet enough times. \nTouch count = {}, required {} or more",
            count,
            required_touches);
    } else {
        double z = sum_z / count;

        std::vector<movement_cmd> goto_tubesheet_seq;
        movement_cmd goto_tubesheet;
        goto_tubesheet.axes = "Z";
        goto_tubesheet.first_axis_setpoint = z;
        goto_tubesheet.second_axis_setpoint = 0;
        goto_tubesheet_seq.push_back(goto_tubesheet);

        if (!rema_instance.execute_sequence(goto_tubesheet_seq)) {
            res["error"] = "Executing sequence";
            status = restbed::RESET_CONTENT;
        } else {
            auto step = goto_tubesheet_seq.begin();
            if (step->executed && step->execution_results.stopped_on_condition) {
                rema_instance.set_home_z(0);
            }
        }
    }

    close_rest_session(rest_session, status, res);
}

void aligned_tubesheet_get(const std::shared_ptr<restbed::Session> &rest_session) {
    close_rest_session(rest_session, restbed::OK, nlohmann::json(current_session.calculate_aligned_tubes()));
}

// @formatter:off
void restfull_api_create_endpoints(restbed::Service &service) {
    std::map<std::string, std::vector<ResourceEntry>> rest_resources = {
        { "REMA/connect", { { "POST", &REMA_connect } } },
        { "REMA/info", { { "GET", &REMA_info } } },
        { "HXs", { { "GET", &HXs_list } } },
        { "HXs/{HX_name: .*}", { { "DELETE", &HXs_delete } } },
        { "HXs/tubesheet/load", { { "GET", &HXs_tubesheet_load } } },
        { "plans", { { "GET", &plans } } },
        { "plans/{plan: .*}", { { "GET", &plans }, { "DELETE", &plans_delete } } },
        { "tools",
          {
              { "GET", &tools_list },
              { "POST", &tools_create },
          } },
        { "tools/{tool_name: .*}", { { "DELETE", &tools_delete } } },
        { "tools/{tool_name: .*}/select", { { "POST", &tools_select } } },
        {
            "sessions",
            {
                { "GET", &sessions_list },
                { "POST", &sessions_create },
            },
        },
        { "sessions/{session_name: .*}", { { "GET", &sessions_load }, { "DELETE", &sessions_delete } } },
        { "current-session/info", { { "GET", &sessions_info } } },
        { "calibration-points", { { "GET", &cal_points_list } } },
        { "calibration-points/{tube_id: .*}", { { "PUT", &cal_points_add_update }, { "DELETE", &cal_points_delete } } },
        { "tubes/{tube_id: .*}", { { "PUT", &tubes_set_status } } },
        { "go-to-tube/{tube_id: .*}", { { "GET", &go_to_tube } } },
        { "move-free-run/{dir: .*}", { { "GET", &move_free_run } } },
        { "move-incremental", { { "POST", &move_incremental } } },
        { "determine-tube-center/{tube_id: .*}/{set_home: .*}", { { "GET", &determine_tube_center } } },
        { "set-home-xy/", { { "GET", &set_home_xy } } },
        { "set-home-xy/{tube_id: .*}", { { "GET", &set_home_xy } } },
        { "determine-tubesheet-z", { { "GET", &determine_tubesheet_z } } },
        { "set-home-z/{z: .*}", { { "GET", &set_home_z } } },
        { "aligned-tubesheet-get", { { "GET", &aligned_tubesheet_get } } },
        { "axes-hard-stop-all", { { "GET", &axes_hard_stop_all } } },
        { "axes-soft-stop-all", { { "GET", &axes_soft_stop_all } } },
        { "change-network-settings", { { "POST", &change_network_settings } } },

    };
    // @formatter:on

    // SPDLOG_INFO("Creando endpoints");
    for (auto [path, resources] : rest_resources) {
        auto resource_rest = std::make_shared<restbed::Resource>();
        resource_rest->set_path(std::string("/REST/").append(path));
        // resource_rest->set_failed_filter_validation_handler(
        //         failed_filter_validation_handler);

        for (ResourceEntry &res : resources) {
            resource_rest->set_method_handler(res.method, res.callback);
        }
        service.publish(resource_rest);
    }
}
