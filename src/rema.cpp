#include <algorithm>
#include <csv.h>
#include <iostream>
#include <vector>

#include <fstream>
#include <json.hpp>
#include <map>
#include <spdlog/spdlog.h>
#include <string>
#include <memory>

#include "rema.hpp"
#include "session.hpp"
#include "tool.hpp"

const std::filesystem::path config_file_path = "config.json";
const std::filesystem::path rema_dir = std::filesystem::path("rema");
const std::filesystem::path tools_dir = rema_dir / "tools";

std::map<std::string, Tool> REMA::tools;

void REMA::cancel_sequence_in_progress() {
    while (is_sequence_in_progress) {
        cancel_sequence = true;
    }
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

void REMA::connect(const std::string &rtu_host,int rtu_port) {
    rtu_host_ = rtu_host;
    rtu_port_ = rtu_port;
    if (command_client.connect(rtu_host, rtu_port) < 0) {
        SPDLOG_WARN("Unable to connect to Command endpoint");
    };
    
    if (telemetry_client.connect(rtu_host, rtu_port + 1) < 0) {
        SPDLOG_WARN("Unable to connect to Telemetry endpoint");
    } else {
        telemetry_client.start();
    }    
}

void REMA::reconnect() {
    command_client.close();
    telemetry_client.close();
    connect(rtu_host_, rtu_port_);
}

void REMA::update_telemetry(std::string &stream) {
    nlohmann::json json;
    try {
        std::lock_guard<std::mutex> lock(mtx);
        if (!stream.empty()) {
            json = nlohmann::json::parse(stream);

            if (json.contains("telemetry")) {
                telemetry = json["telemetry"];
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
    execute_command({ { "command", "SET_COORDS" }, { "pars", { { "position_x", x }, { "position_y", y } } } });
}

void REMA::set_home_z(double z) {
    execute_command({ { "command", "SET_COORDS" },
                      { "pars",
                        {
                            { "position_z", z },
                        } } });
}

nlohmann::json REMA::execute_command(const nlohmann::json command) { // do not change command to a reference
    nlohmann::json to_rema;
    to_rema["commands"].push_back(command);
    std::string tx_buffer = to_rema.dump();

    SPDLOG_INFO("Sending to REMA: {}", tx_buffer);
    command_client.send_request(tx_buffer);
    return nlohmann::json::parse(command_client.get_response());
}

void REMA::execute_command_no_wait(const nlohmann::json command) { // do not change command to a reference
    nlohmann::json to_rema;
    to_rema["commands"].push_back(command);
    std::string tx_buffer = to_rema.dump();

    SPDLOG_INFO("Enviando a REMA: {}", tx_buffer);
    command_client.send_request(tx_buffer);
}

nlohmann::json REMA::move_closed_loop(movement_cmd cmd) {
    return execute_command({ { "command", "MOVE_CLOSED_LOOP" },
                            { "pars",
                                { { "axes", cmd.axes },
                                { "first_axis_setpoint", cmd.first_axis_setpoint },
                                { "second_axis_setpoint", cmd.second_axis_setpoint } } } });
}

void REMA::axes_hard_stop_all() {
    cancel_sequence_in_progress();
    execute_command({ { "command", "AXES_HARD_STOP_ALL" } });
}

void REMA::axes_soft_stop_all() {
    cancel_sequence_in_progress();
    execute_command({ { "command", "AXES_SOFT_STOP_ALL" } });
}

bool REMA::execute_sequence(std::vector<movement_cmd> &sequence) {
    cancel_sequence_in_progress();
    cancel_sequence = false;
    is_sequence_in_progress = true;
    bool was_completed = true;

    execute_command({ { "command", "AXES_SOFT_STOP_ALL" } });

    // Create an individual command object and add it to the array
    for (auto &step : sequence) {
        move_closed_loop(step);
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait for telemetry update...

        bool stopped_on_probe = false;
        bool stopped_on_condition = false;
        do {
            if (step.axes == "XY") {
                stopped_on_probe = telemetry.probe.x_y;
                stopped_on_condition = telemetry.on_condition.x_y;
            } else {
                stopped_on_probe = telemetry.probe.z;
                stopped_on_condition = telemetry.on_condition.z;
            }
        } while (!(stopped_on_probe || stopped_on_condition || cancel_sequence));

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
