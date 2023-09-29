#include <open3d/Open3D.h>
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
#include "touch_probe.hpp"

extern TouchProbeFSM tpFSM;

std::map<std::string, Tool> REMA::tools;

void REMA::cancel_sequence_in_progress() {
    while (is_sequence_in_progress) {
        cancel_sequence = true;
    }
    cancel_sequence = false;
}

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
                tpFSM.process(telemetry.limits.probe);
                telemetry.limits.debounced_probe = tpFSM.isTouchDetected();
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
    tpFSM.newCommand();
    to_rema["commands"].push_back(command);
    std::string tx_buffer = to_rema.dump();

    std::cout << "Enviando a RTU: " << tx_buffer << "\n";
    command_client.send_blocking(tx_buffer);
    command_client.receive_blocking();
}

void REMA::move_closed_loop(movement_cmd cmd) {
    tpFSM.newCommand();
    execute_command({ { "command", "MOVE_CLOSED_LOOP" },
        { "pars",
                { { "axes", cmd.axes },
                  { "first_axis_setpoint", cmd.first_axis_setpoint },
                  { "second_axis_setpoint", cmd.second_axis_setpoint } } }
    });
}

void REMA::axes_hard_stop_all() {
    cancel_sequence_in_progress();
    execute_command({ { "command", "AXES_HARD_STOP_ALL" }});
}

void REMA::axes_soft_stop_all() {
    cancel_sequence_in_progress();
    execute_command({ { "command", "AXES_SOFT_STOP_ALL" }});
}

bool REMA::execute_sequence(std::vector<movement_cmd>& sequence) {
    cancel_sequence_in_progress();
    is_sequence_in_progress = true;
    bool was_completed = true;

    execute_command({ { "command", "AXES_SOFT_STOP_ALL" }});

    // Create an individual command object and add it to the array
    for (auto &step : sequence) {
        move_closed_loop(step);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));                     // Wait for telemetry update...

        bool stopped_on_probe = false;
        bool stopped_on_condition = false;
        do {
            try {
                telemetry_client.receive_async([this](std::string &rx_buffer) {this->update_telemetry(rx_buffer); });
                std::this_thread::sleep_for(std::chrono::milliseconds(10));                     // Wait for telemetry update...
            } catch (std::exception &e) {                // handle exception
                std::cerr << e.what() << "\n";
            }

            stopped_on_probe = telemetry.limits.debounced_probe && step.stop_on_probe;

            if (step.axes == "XY") {
                stopped_on_condition = telemetry.on_condition.x_y && step.stop_on_condition;
            } else {
                stopped_on_condition = telemetry.on_condition.z && step.stop_on_condition;
            }
        } while (!(stopped_on_probe || stopped_on_condition || cancel_sequence ));

        if (cancel_sequence) {
            was_completed = false;
            break;
        } else {
                step.executed = true;
                step.execution_results.coords = telemetry.coords;
                step.execution_results.stopped_on_probe = stopped_on_probe;
                step.execution_results.stopped_on_condition = stopped_on_condition;
        }
    }
    is_sequence_in_progress = false;
    cancel_sequence = false;
    return was_completed;
}
