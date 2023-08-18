#include <functional>
#include <ctime>
#include <thread>
#include <chrono>
#include <fstream>
#include <iostream>
#include <exception>
#include <vector>

#include "inc/json.hpp"
#include "inc/inspection-session.hpp"
#include "inc/tool.hpp"
#include "inc/rema.hpp"
#include "inc/HXs.hpp"
#include "inc/circle_fit.hpp"

using namespace std::chrono_literals;
extern InspectionSession current_session;
extern REMA rema;

/**
 * REMA related functions
 **/

nlohmann::json rema_connect_cmd(nlohmann::json pars) {
    nlohmann::json res;
    try {
        REMA &rema_instance = REMA::get_instance();
        rema_instance.rtu.connect_comm();
        rema_instance.rtu.connect_telemetry();
        res["ACK"] = true;
    } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
        res["ERROR"] = e.what();
    }
    return res;
}

nlohmann::json rema_info_cmd(nlohmann::json pars) {
    REMA &rema_instance = REMA::get_instance();

    nlohmann::json res = nlohmann::json(nlohmann::json::value_t::object);
    res["tools"] = REMA::tools_list();
    res["last_selected_tool"] = rema_instance.last_selected_tool;
    return res;
}

/**
 * HX related functions
 **/

nlohmann::json hx_tubesheet_load_cmd(nlohmann::json pars) {
    return HX_get_tubes(current_session.tubesheet_csv);
}

nlohmann::json hx_list_cmd(nlohmann::json pars) {
    return HXs_list();
}

/**
 * Tools related functions
 **/

nlohmann::json tool_create_cmd(nlohmann::json pars) {
    nlohmann::json res;

    std::string tool_name = pars["tool_name"];
    if (tool_name.empty()) {
        res["success"] = false;
        res["logs"] = "no filename specified";
        return res;
    }

    float offset_x = std::stof(pars["offset_x"].get<std::string>());
    float offset_y = std::stof(pars["offset_y"].get<std::string>());
    float offset_z = std::stof(pars["offset_z"].get<std::string>());

    try {
        Tool new_tool(tool_name, offset_x, offset_y, offset_z);
        res["success"] = true;
    } catch (const std::exception &e) {
        res["success"] = false;
        res["logs"] = e.what();
    }
    return res;
}


nlohmann::json tools_list_cmd(nlohmann::json pars) {
    return REMA::tools_list();
}

nlohmann::json tool_select_cmd(nlohmann::json pars) {
    if (pars["tool_name"].is_string()) {
        std::string tool_name = pars["tool_name"];
        REMA &rema_instance = REMA::get_instance();
        rema_instance.set_selected_tool(tool_name);
    }
    return nlohmann::json();
}

nlohmann::json tool_delete_cmd(nlohmann::json pars) {
    std::string tool_name = pars["tool_name"];

    nlohmann::json res = nlohmann::json(nlohmann::json::value_t::object);
    try {
        REMA::delete_tool(tool_name);
        res["success"] = true;
    } catch (const std::filesystem::filesystem_error &e) {
        res["logs"] = std::string("filesystem error: ") + e.what();
    }
    return res;
}

/**
 * Inspection Plans related functions
 **/

nlohmann::json insp_plan_load_cmd(nlohmann::json pars) {
    std::string insp_plan;
    if (pars["insp_plan"].is_string()) {
        insp_plan = pars["insp_plan"];
    }
    return current_session.inspection_plan_get(insp_plan);
}

/**
 * Inspection Sessions related functions
 **/

nlohmann::json insp_sessions_list_cmd(nlohmann::json pars) {
    return InspectionSession::sessions_list();
}

nlohmann::json session_create_cmd(nlohmann::json pars) {
    std::string session_name = pars["session_name"];

    nlohmann::json res;

    if (session_name.empty()) {
        res["success"] = false;
        res["logs"] = "no filename specified";
        return res;
    }

    if (pars["hx"].empty()) {
        res["success"] = false;
        res["logs"] = "no HX selected";
        return res;
    }

    try {

        InspectionSession new_session(session_name, std::filesystem::path(pars["hx"]));
        res["logs"] = new_session.load_plans();
        new_session.save_to_disk();

        current_session = new_session;
        res["success"] = true;
        return res;

    } catch (const std::exception &e) {
        res["success"] = false;
        res["logs"] = e.what();
    }
    return res;
}

nlohmann::json session_load_cmd(nlohmann::json pars) {
    std::string session_name = pars["session_name"];
    nlohmann::json res;

    if (session_name.empty()) {
        res["success"] = false;
        res["logs"] = "no session selected";
        return res;
    }

    try {
        current_session.load(session_name);
        res["success"] = true;
    } catch (std::exception &e) {
        res["logs"] = e.what();
    }
    return res;
}

nlohmann::json session_info_cmd(nlohmann::json pars) {
    nlohmann::json res = nlohmann::json(nlohmann::json::value_t::object);

    if (current_session.is_loaded()) {
        res = current_session;
        res["is_loaded"] = true;
        return res;
    }
    return res;
}

nlohmann::json session_delete_cmd(nlohmann::json pars) {
    std::string session_name = pars["session_name"];
    nlohmann::json res = nlohmann::json(nlohmann::json::value_t::object);

    try {
        InspectionSession::delete_session(session_name);
        res["success"] = true;
    } catch (const std::filesystem::filesystem_error &e) {
        res["logs"] = std::string("filesystem error: ") + e.what();
    }
    return res;
}

nlohmann::json tube_set_status_cmd(nlohmann::json pars) {
    nlohmann::json res;

    std::string insp_plan = pars["insp_plan"];
    std::string tube_id = pars["tube_id"];
    bool checked = pars["checked"];

    current_session.set_tube_inspected(insp_plan, tube_id, checked);
    res[tube_id] = checked;
    return res;
}

struct sequence_step {
    std::string axes;
    double first_axis_setpoint;
    double second_axis_setpoint;
    struct Point3D reached_coords;
};

nlohmann::json tube_determine_center_cmd(nlohmann::json pars) {

    std::vector<sequence_step> seq = {
            {
                    "XY",           //Tube 11
                    0.562598,
                    0.324803,
            },
            {
                    "XY",           //Tube 42           ( The center of the three coordinates shoud be at X = 1.125197, Y = 0.649606 Tube 22)
                    1.125197,
                    1.299213
            },
            {
                    "XY",           //Tube 12
                    1.687795,
                    0.324803
            },
    };

    nlohmann::json res;
    REMA &rema_instance = REMA::get_instance();
    std::string tx_buffer;
    nlohmann::json to_rema;
    std::vector<Point3D> tube_boundary_points;

    // Create an individual command object and add it to the array
    for (auto &seq_step : seq) {
        nlohmann::json command = {
            {"command", "MOVE_CLOSED_LOOP"},
            {"pars", {
                {"axes", seq_step.axes},
                {"first_axis_setpoint", seq_step.first_axis_setpoint},
                {"second_axis_setpoint", seq_step.second_axis_setpoint}
            }}
        };

        to_rema["commands"].clear();
        to_rema["commands"].push_back(command);
        tx_buffer = to_rema.dump();

        std::cout << "Enviando a RTU: "<< tx_buffer << "\n";
        rema_instance.rtu.send(tx_buffer);
        //std::this_thread::sleep_for(std::chrono::milliseconds(100));        // Wait for telemetry to update

        // Using std::bind to bind the member function to an instance
        // auto bound_member_function = std::bind(&REMA::update_telemetry_callback_method, &rema_instance, std::placeholders::_1);

//        int count = 0;
//        int maxTries = 3;
//        while(true) {
//            try {
//                rema_instance.rtu.receive_telemetry([&rema_instance](auto &rx_buffer) {rema_instance.update_telemetry_callback_method(rx_buffer); });
//                break;
//            } catch (std::exception &e) {                // handle exception
//                if (++count == maxTries) {
//                    return res;
//                }
//            }
//        }

        do {
            try {
                boost::asio::streambuf rx_buffer;
                rema_instance.rtu.receive_telemetry_sync(rx_buffer);
                std::string stream(
                        boost::asio::buffer_cast<const char*>(
                                (rx_buffer).data()));

                std::cout << stream << "\n";
                rema_instance.update_telemetry_callback_method(rx_buffer);
            } catch (std::exception &e) {                // handle exception
                std::cerr << e.what() << "\n";
                return res;
            }

        } while (!(rema_instance.telemetry.limits.probe || rema_instance.cancel_cmd || rema_instance.telemetry.on_condition.x_y));

        if (rema_instance.telemetry.on_condition.x_y) {       // ask for probe_touching
            tube_boundary_points.push_back(rema_instance.telemetry.coords);
        }
    }
    res["reached_coords"] = tube_boundary_points;
    auto [center, radius] = fitCircle(tube_boundary_points);
    res["center"] = center;
    res["radius"] = radius;

    return res;
}


// @formatter:off
std::map<std::string, std::function<nlohmann::json(nlohmann::json)>> commands =
		{ { "rema_connect", &rema_connect_cmd },
		  { "rema_info", &rema_info_cmd },
          { "hx_tubesheet_load", &hx_tubesheet_load_cmd },
		  { "hx_list", &hx_list_cmd },
          { "tool_create", &tool_create_cmd },
		  { "tools_list", &tools_list_cmd },
		  { "tool_select", &tool_select_cmd },
		  { "tool_delete", &tool_delete_cmd },
		  { "insp_plan_load", &insp_plan_load_cmd },
		  { "insp_sessions_list", &insp_sessions_list_cmd },
		  { "session_create", &session_create_cmd },
		  { "session_load", &session_load_cmd },
		  { "session_info", &session_info_cmd },
		  { "session_delete", &session_delete_cmd },
		  { "tube_set_status", &tube_set_status_cmd },
		  { "tube_determine_center", &tube_determine_center_cmd },
		};
// @formatter:on

nlohmann::json cmd_execute(std::string command, nlohmann::json par) {
    nlohmann::json res;
    if (auto cmd_entry = commands.find(command); cmd_entry != commands.end()) {
        res = (*cmd_entry).second(par);
    }
    return res;
}
