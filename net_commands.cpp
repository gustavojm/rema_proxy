#include <functional>
#include <ctime>
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

nlohmann::json tube_determine_center_cmd(nlohmann::json pars) {
    nlohmann::json res;
    REMA &rema_instance = REMA::get_instance();
    std::string tx_buffer;

    nlohmann::json to_rema;
    nlohmann::json commands = nlohmann::json::array();

    // Create an individual command object and add it to the array
    nlohmann::json command1 = {
        {"command", "MOVE_CLOSED_LOOP"},
        {"pars", {
            {"axes", "XY"},
            {"first_axis_setpoint", 100},
            {"second_axis_setpoint", 200}
        }}
    };
    commands.push_back(command1);
    to_rema["commands"] = commands;
    tx_buffer = to_rema.dump();
    std::cout << "Enviando a RTU: "<< tx_buffer << "\n";
    rema_instance.rtu.send(tx_buffer);
    sleep(5);

    nlohmann::json command2 = {
            {"command", "MOVE_CLOSED_LOOP"},
            {"pars", {
                {"axes", "XY"},
                {"first_axis_setpoint", -300},
                {"second_axis_setpoint", -500}
            }}
    };
    commands.push_back(command1);
    to_rema["commands"] = commands;
    tx_buffer = to_rema.dump();
    std::cout << "Enviando a RTU: "<< tx_buffer << "\n";
    rema_instance.rtu.send(tx_buffer);
    sleep(5);

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
