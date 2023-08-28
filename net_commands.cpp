#include <functional>
#include <ctime>
#include <thread>
#include <chrono>
#include <fstream>
#include <iostream>
#include <exception>
#include <vector>

#include "json.hpp"
#include "inspection-session.hpp"
#include "tool.hpp"
#include "rema.hpp"
#include "HXs.hpp"
#include "circle_fit.hpp"

using namespace std::chrono_literals;
extern InspectionSession current_session;

nlohmann::json session_delete_cmd(nlohmann::json pars) {
    nlohmann::json res = nlohmann::json(nlohmann::json::value_t::object);
    std::string session_name = pars["session_name"];

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



nlohmann::json cal_points_list_cmd(nlohmann::json pars) {
    std::vector<CalPointEntryWithTubeID> res;

    for (auto [key, value] : current_session.cal_points ) {
        CalPointEntryWithTubeID entry;
        entry.tube_id = key;
        entry.ideal_coords = value.ideal_coords;
        entry.determined_coords = value.determined_coords;
        entry.determined = value.determined;
        res.push_back(entry);
    }

    return res;
}

nlohmann::json cal_point_add_cmd(nlohmann::json pars) {
    nlohmann::json res;

    std::string id = pars["id"];

    if (id.empty()) {
        res["success"] = false;
        res["logs"] = "no tube specified";
        return res;
    }

    CalPointEntry cpe = {
            {5, 5, 5},
            {10, 15, 25},
            true,
    };
    current_session.cal_points[id] = cpe;

    res["success"] = true;
    return res;
}

nlohmann::json cal_point_delete_cmd(nlohmann::json pars) {
    nlohmann::json res;

    std::string tube_id = pars["tube_id"];

    if (tube_id.empty()) {
        res["success"] = false;
        res["logs"] = "no tube specified";
        return res;
    }
    current_session.cal_points.erase(tube_id);

    res["success"] = true;
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
    REMA &rema_instance = REMA::get_instance();//extern REMA rema;

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

        do {
            try {
                boost::asio::streambuf rx_buffer;
                rema_instance.rtu.receive_telemetry_sync(rx_buffer);
                std::string stream(
                        boost::asio::buffer_cast<const char*>(
                                (rx_buffer).data()));

                std::cout << stream << "\n";
                rema_instance.update_telemetry(rx_buffer);
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

    std::vector<Point3D> src_dst_points = {center};

    res["transformed_points"] = rema_instance.get_aligned_tubes(current_session, src_dst_points, src_dst_points);

    return res;
}


// @formatter:off
std::map<std::string, std::function<nlohmann::json(nlohmann::json)>> commands =
		{
		  { "session_delete", &session_delete_cmd },
		  { "cal_points_list", &cal_points_list_cmd },
		  { "cal_point_add", &cal_point_add_cmd },
		  { "cal_point_delete", &cal_point_delete_cmd },
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
