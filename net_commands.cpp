#include <ciaa.hpp>
#include <functional>
#include <ctime>
#include <chrono>
#include <fstream>
#include <iostream>
#include <exception>
#include <vector>

#include "inc/csv.h"
#include "inc/json.hpp"
#include "inc/inspection-session.hpp"
#include "inc/tool.hpp"
#include "inc/rema.hpp"

using namespace std::chrono_literals;
extern InspectionSession current_session;
extern REMA rema;

struct Tube {
    std::string x_label;
    std::string y_label;
    float cl_x;
    float cl_y;
    float hl_x;
    float hl_y;
    int tube_id;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Tube, x_label, y_label, cl_x, cl_y, hl_x,
        hl_y, tube_id)

/**
 * REMA related functions
 **/

nlohmann::json rema_connect_cmd(nlohmann::json pars) {
    nlohmann::json res;
    try {
        REMA &rema_instance = REMA::get_instance();
        rema_instance.rtu.connect();
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
    // Parse the CSV file to extract the data for each tube
    try {
        std::vector<Tube> tubes;
        io::CSVReader<7, io::trim_chars<' ', '\t'>, io::no_quote_escape<';'>> in(
                current_session.tubesheet_csv);
        in.read_header(io::ignore_extra_column, "x_label", "y_label", "cl_x",
                "cl_y", "hl_x", "hl_y", "tube_id");
        std::string x_label, y_label;
        float cl_x, cl_y, hl_x, hl_y;
        std::string tube_id;
        while (in.read_row(x_label, y_label, cl_x, cl_y, hl_x, hl_y, tube_id)) {
            tubes.push_back( { x_label, y_label, cl_x, cl_y, hl_x, hl_y,
                    std::stoi(tube_id.substr(5)) });
        }

        nlohmann::json res(tubes); //requires to_json and from_json to be defined to be able to serialize the custom object "tube"
        return res;
    } catch (std::exception &e) {
        nlohmann::json res(nlohmann::json::value_t::object);
        return res;
    }
}

nlohmann::json hx_list_cmd(nlohmann::json pars) {
    nlohmann::json res;

    std::string path = "./HXs";
    for (const auto &entry : std::filesystem::directory_iterator(path)) {
        if (entry.is_directory()) {
            res.push_back( { { "value", entry.path().filename() }, { "text",
                    entry.path().filename() } });
        }
    }
    std::sort(res.begin(), res.end());

    return res;
}

/**
 * Tools related functions
 **/

nlohmann::json tools_list_cmd(nlohmann::json pars) {
    nlohmann::json res;
    res["tools"] = REMA::tools_list();
    return res;
}

nlohmann::json tool_select_cmd(nlohmann::json pars) {
    std::string tool_name = std::filesystem::path(pars["tool_name"]);

    REMA &rema_instance = REMA::get_instance();
    rema_instance.set_selected_tool(tool_name);

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
    nlohmann::json res; //requires to_json and from_json to be defined to be able to serialize the custom object "tube"

    try {
        std::string insp_plan = pars["insp_plan"];
        current_session.last_selected_plan = insp_plan;

        for (auto [key, value] : current_session.insp_plans.at(insp_plan)) {
            res.push_back( { { "tube_id", key }, { "col", value.col }, { "row",
                    value.row }, { "inspected", value.inspected } });
        }

    } catch (nlohmann::json::exception &e) {
    }
    return res;
}

/**
 * Inspection Sessions related functions
 **/

nlohmann::json insp_sessions_list_cmd(nlohmann::json pars) {
    return InspectionSession::sessions_list();
}

nlohmann::json session_create_cmd(nlohmann::json pars) {
    std::filesystem::path session_name = std::filesystem::path(
            pars["session_name"]);

    nlohmann::json res;

    if (session_name.empty() && !session_name.has_filename()) {
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
    std::filesystem::path session_name = std::filesystem::path(
            pars["session_name"]);
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
    std::filesystem::path session_name = std::filesystem::path(
            pars["session_name"]);
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

    try {
        std::string insp_plan = pars["insp_plan"];
        std::string tube_id = pars["tube_id"];
        bool checked = pars["checked"];

        current_session.set_tube_inspected(insp_plan, tube_id, checked);
        res[tube_id] = checked;
    } catch (nlohmann::json::exception &e) {
        res["logs"] = e.what();
    }
    return res;
}

// @formatter:off
std::map<std::string, std::function<nlohmann::json(nlohmann::json)>> commands =
		{ { "rema_connect", &rema_connect_cmd },
		  { "rema_info", &rema_info_cmd },
          { "hx_tubesheet_load", &hx_tubesheet_load_cmd },
		  { "hx_list", &hx_list_cmd },
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
		};
// @formatter:on

nlohmann::json cmd_execute(std::string command, nlohmann::json par) {
    nlohmann::json res;
    if (auto cmd_entry = commands.find(command); cmd_entry != commands.end()) {
        res = (*cmd_entry).second(par);
    }
    return res;
}
