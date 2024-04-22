#include <iostream>
#include <vector>
#include <algorithm>
#include <csv.h>

#include <fstream>
#include <string>
#include <map>
#include <json.hpp>
#include <spdlog/spdlog.h>

#include "rema.hpp"
#include "inspection-session.hpp"
#include "tool.hpp"
#include "touch_probe.hpp"

extern TouchProbeFSM tpFSM;

const inline std::filesystem::path config_file_path = "config.json";
const inline std::filesystem::path rema_dir = std::filesystem::path("rema");
const inline std::filesystem::path tools_dir = rema_dir / "tools";

std::map<std::string, Tool> REMA::tools;

void REMA::cancel_sequence_in_progress() {
    while (is_sequence_in_progress) {
        cancel_sequence = true;
    }
    cancel_sequence = false;
}

void REMA::load_config() {
    try {                
        std::ifstream config_file(config_file_path);
        if (config_file.is_open()) {
            config_file >> config;
            this->last_selected_tool = config["REMA"]["last_selected_tool"];
        } else {
            SPDLOG_WARN("{} not found", config_file_path.string());
            std::exit(1);
        }
    } catch (std::exception &e) {
        SPDLOG_WARN(e.what());
    }
}

void REMA::save_config() {
    std::ofstream file(config_file_path);
    // Default JSON deserialization not possible because REMA is not default constructible (to enforce singleton pattern)
    config["REMA"]["last_selected_tool"] = last_selected_tool;
    file << config;
}

void REMA::connect(const std::string &rtu_host, const std::string &rtu_service) {
    command_client.set_host(rtu_host);
    command_client.set_service(rtu_service);

    telemetry_client.set_host(rtu_host);
    telemetry_client.set_service(std::to_string(std::stoi(rtu_service) + 1));

    command_client.connect();
    telemetry_client.connect();
}

void REMA::update_telemetry(std::string &stream) {
    nlohmann::json json;
    try {
        std::lock_guard<std::mutex> lock(mtx);
        if (!stream.empty()) {
            json = nlohmann::json::parse(stream);

            if (json.contains("telemetry")) {
                telemetry = json["telemetry"];
                tpFSM.process(!telemetry.limits.probe);
                telemetry.limits.debounced_probe = tpFSM.isTouchDetected();
            }

            if (json.contains("temps")) {
                temps = json["temps"];
            }
        }
    } catch (std::exception &e) {
        SPDLOG_ERROR("TELEMETRY COMMUNICATIONS ERROR {}", e.what());
    }
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

void REMA::execute_command(const nlohmann::json command) {      // do not change command to a reference
    nlohmann::json to_rema;
    tpFSM.newCommand();
    to_rema["commands"].push_back(command);
    std::string tx_buffer = to_rema.dump();

    SPDLOG_INFO("Sending to REMA: {}", tx_buffer);
    command_client.send_blocking(tx_buffer);
    command_client.receive_blocking();
}

void REMA::execute_command_no_wait(const nlohmann::json command) {      // do not change command to a reference
    nlohmann::json to_rema;
    tpFSM.newCommand();
    to_rema["commands"].push_back(command);
    std::string tx_buffer = to_rema.dump();

    SPDLOG_INFO("Enviando a REMA: {}", tx_buffer);
    command_client.send_blocking(tx_buffer);
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
                SPDLOG_ERROR(e.what());
            }

            stopped_on_probe = telemetry.limits.debounced_probe && step.stop_on_probe;

            if (step.axes == "XY") {
                stopped_on_condition = telemetry.on_condition.x_y && step.stop_on_condition;
            } else {
                stopped_on_condition = telemetry.on_condition.z && step.stop_on_condition;
            }
        } while (!(stopped_on_probe || stopped_on_condition || cancel_sequence ));

        if (!cancel_sequence) {
            step.executed = true;
            step.execution_results.coords = telemetry.coords;
            step.execution_results.stopped_on_probe = stopped_on_probe;
            step.execution_results.stopped_on_condition = stopped_on_condition;
        } else {
            was_completed = false;
            break;
        }
    }
    is_sequence_in_progress = false;
    cancel_sequence = false;
    return was_completed;
}
