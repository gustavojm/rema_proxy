#include <Open3D/Geometry/PointCloud.h>
#include <Open3D/Registration/ColoredICP.h>
#include <Open3D/IO/ClassIO/ImageIO.h>
#include <Eigen/Eigen>
#include <iostream>
#include <vector>
#include <algorithm>
#include <csv.h>

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <csv.h>
#include <json.hpp>

#include "rema.hpp"
#include "inspection-session.hpp"
#include "tool.hpp"

void REMA::save_to_disk() const {
    std::ofstream file(rema_file);
    nlohmann::json j;

    // Default JSON deserialization not possible because REMA is not default constructible (to enforce singleton pattern)
    j["last_selected_tool"] = last_selected_tool;
    file << j;
}

void REMA::update_telemetry(std::string &stream) {
    nlohmann::json json;
    try {
        std::lock_guard<std::mutex> lock(mtx);
        if (!stream.empty()) {
            //cout << stream << endl;
            json = nlohmann::json::parse(stream);

            if (json.contains("telemetry")) {
                telemetry = json["telemetry"];
            }

            if (json.contains("temps")) {
                temps = json["temps"];
            }
        }
    } catch (std::exception &e) {
        std::string message = std::string(e.what());
        std::cerr << "TELEMETRY COMMUNICATIONS ERROR " << message << "\n";
    }
    return;
}

void REMA::set_home_xy(double x, double y) {
    execute_command({ { "command", "SET_COORDS" },
        { "pars",
                { { "position_x", x },
                  { "position_y", y }
                }
        }});
}

void REMA::set_home_z(double z) {
    execute_command({ { "command", "SET_COORDS" },
        { "pars",
                { { "position_z", z },
                }
        }});
}

void REMA::execute_command(nlohmann::json command) {
    nlohmann::json to_rema;
    to_rema["commands"].push_back(command);
    std::string tx_buffer = to_rema.dump();

    std::cout << "Enviando a RTU: " << tx_buffer << "\n";
    command_client.send_blocking(tx_buffer);
    command_client.receive_blocking();
}

void REMA::move_closed_loop(sequence_step step) {
    tpFSM.newCommand();
    execute_command({ { "command", "MOVE_CLOSED_LOOP" },
        { "pars",
                { { "axes", step.axes },
                  { "first_axis_setpoint", step.first_axis_setpoint },
                  { "second_axis_setpoint", step.second_axis_setpoint } } }
    });
}

void REMA::execute_sequence(std::vector<sequence_step>& sequence) {
    while (is_sequence_in_progress) {
        cancel_sequence = true;
    }
    cancel_sequence = false;
    is_sequence_in_progress = true;

    execute_command({ { "command", "AXES_SOFT_STOP_ALL" }});

    // Create an individual command object and add it to the array
    for (auto &step : sequence) {
        move_closed_loop(step);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));                     // Wait for telemetry update...

        bool stopped_on_probe = false;
        bool stopped_on_condition = false;
        do {
            try {
                std::string stream = telemetry_client.receive_blocking();
                update_telemetry(stream);
            } catch (std::exception &e) {                // handle exception
                std::cerr << e.what() << "\n";
            }

            stopped_on_probe = telemetry.limits.probe && step.stop_on_probe;

            if (step.axes == "XY") {
                stopped_on_condition = telemetry.on_condition.x_y && step.stop_on_condition;
            } else {
                stopped_on_condition = telemetry.on_condition.z && step.stop_on_condition;
            }
        } while (!(stopped_on_probe || stopped_on_condition || cancel_sequence ));

        if (cancel_sequence) {
            break;
        } else {
                step.executed = true;
                step.execution_results.coords = telemetry.coords;
                step.execution_results.stopped_on_probe = stopped_on_probe;
                step.execution_results.stopped_on_condition = stopped_on_condition;
        }
        is_sequence_in_progress = false;
    }
}
