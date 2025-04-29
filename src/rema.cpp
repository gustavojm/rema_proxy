#include <algorithm>
#include <csv.hpp>
#include <iostream>
#include <vector>

#include <fstream>
#include <map>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <variant>

#include "nlohmann/json.hpp"
#include "rema.hpp"
#include "session.hpp"
#include "chart.hpp"
#include "tool.hpp"

REMA::REMA() {

    spdlog::set_pattern(log_pattern);

    telemetry_client.set_on_receive_callback(
        [&](std::vector<uint8_t>& line) { 
            update_telemetry(line);
        }
    );

    logs_client.set_on_receive_callback(
        [&](std::string& line) { 
            save_logs(line); 
        }
    );

    auto now = to_time_t(std::chrono::steady_clock::now());
    std::filesystem::path log_file = logs_dir / ("log" + std::to_string(now) + ".json");
    
    try {           
        if (!std::filesystem::exists(logs_dir)) {
            std::filesystem::create_directories(logs_dir);
        }

        logs_ofstream.open(log_file, std::ios::app);
        SPDLOG_INFO("Saving logs to ./{}", log_file.string());

        load_config();
        for (const auto &entry : std::filesystem::directory_iterator(tools_dir)) {
            Tool t(entry.path());
            tools[entry.path().filename().replace_extension()] = t;
        }
        this->loaded = true;
    } catch (std::exception &e) {
        SPDLOG_WARN(e.what());
    }
}

void REMA::add_tool(const Tool &tool) {
    tools[tool.name] = tool;
}

void REMA::delete_tool(const std::string &tool) {
    std::filesystem::remove(tools_dir / (tool + std::string(".json")));
    tools.erase(tool);
}

void REMA::cancel_sequence_in_progress() {
    while (is_sequence_in_progress) {
        cancel_sequence = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Avoid hogging the processor
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

const std::filesystem::path rema_startup_cmds_file_path = "rema_startup.json";

nlohmann::json REMA::send_startup_commands() {
    nlohmann::json res;
    try {
        std::ifstream rema_startup_file(rema_startup_cmds_file_path);
        if (rema_startup_file.is_open()) {
            SPDLOG_INFO("REMA start up file found");
            nlohmann::json rema_startup_cmds;
            rema_startup_file >> rema_startup_cmds;
            for (auto cmd : rema_startup_cmds) {
                res.push_back(execute_command(cmd["cmd"], cmd["pars"]));
            }
        }
    } catch (std::exception &e) {
        SPDLOG_WARN(e.what());
    }
    return res;
}

void REMA::save_config() {
    std::ofstream file(config_file_path);
    // Default JSON deserialization not possible because REMA is not default constructible (to enforce singleton pattern)
    config["REMA"]["last_selected_tool"] = last_selected_tool;
    file << config;
}

void REMA::connect(const std::string &rtu_host, int rtu_port) {
    rtu_host_ = rtu_host;
    rtu_port_ = rtu_port;
    if (command_client.connect(rtu_host, rtu_port) < 0) {
        SPDLOG_WARN("Unable to connect to Command endpoint");
    } else {
        send_startup_commands();
    };

    if (telemetry_client.connect(rtu_host, rtu_port + 1) < 0) {
        SPDLOG_WARN("Unable to connect to Telemetry endpoint");
    } else {
        telemetry_client.start();
    }

    if (logs_client.connect(rtu_host, rtu_port + 2) < 0) {
        SPDLOG_WARN("Unable to connect to Logs endpoint");
    } else {
        logs_client.start();
    }

}

void REMA::reconnect() {
    command_client.close();
    telemetry_client.close();   // this would probably be closed by watchdog
    logs_client.close();
    connect(rtu_host_, rtu_port_);
}

void REMA::update_telemetry(std::vector<uint8_t>& stream) {
    nlohmann::json json;    
    try {
        std::lock_guard<std::mutex> lock(mtx);
        if (!stream.empty()) {
            json = nlohmann::json::from_msgpack(stream);

            if (json.contains("telemetry")) {
                telemetry = json["telemetry"];
                ui_telemetry = rema.telemetry;
                Tool tool = rema.get_selected_tool();
                ui_telemetry.coords = current_session.from_rema_to_ui(rema.telemetry.coords, &tool);
                ui_telemetry.targets = current_session.from_rema_to_ui(rema.telemetry.targets, &tool);

                if (ui_telemetry.joystick_movement.x_y != old_telemetry.joystick_movement.x_y && ui_telemetry.joystick_movement.x_y) {
                    chart.init("joystick_movement_XY");
                }

                if (ui_telemetry.joystick_movement.z != old_telemetry.joystick_movement.z && ui_telemetry.joystick_movement.z) {
                    chart.init("joystick_movement_Z");
                }

                if (ui_telemetry.coords != old_telemetry.coords) {
                    chart.insertData({ui_telemetry.coords});
                }
                old_telemetry = ui_telemetry;
            }

            if (json.contains("temps")) {
                new_temps_available = true;
                temps = json["temps"];
            }
        }
    } catch (std::exception &e) {
        SPDLOG_ERROR("TELEMETRY COMMUNICATIONS ERROR {}", e.what());
    }
}

void REMA::save_logs(std::string &stream) {
    try {
        logs_ofstream << stream << std::endl;
        logs_vector.push_back(stream);
    } catch (std::exception &e) {
        SPDLOG_ERROR("LOGS STORAGE ERROR {}", e.what());
    }
}

void REMA::set_home_xyz(Point3D coords) {
    execute_command("SET_COORDS", { { "position_X", coords.x }, { "position_Y", coords.y }, { "position_Z", coords.z } });
}

void REMA::set_home_xy(double x, double y) {
    execute_command("SET_COORDS", { { "position_X", x }, { "position_Y", y } });
}

void REMA::set_home_z(double z) {
    execute_command("SET_COORDS", { { "position_Z", z } });
}

void REMA::execute_command_no_wait(
    const std::string cmd_name,
    const nlohmann::json pars) { // do not change command to a reference
    nlohmann::json to_rema;

    nlohmann::json command;
    command["cmd"] = cmd_name;
    if (!pars.is_null()) {
        command["pars"] = pars;
    }

    to_rema.push_back(command);
    std::string tx_buffer = to_rema.dump();

    SPDLOG_INFO("Sending to REMA: {}", tx_buffer);
    {
        std::lock_guard<std::mutex> lock(rtu_mutex);
        command_client.send_request(tx_buffer);
    }
}

nlohmann::json
REMA::execute_command(const std::string cmd_name, const nlohmann::json pars) { // do not change command to a reference
    execute_command_no_wait(cmd_name, pars);
    return nlohmann::json::parse(command_client.get_response());
}

nlohmann::json REMA::move_closed_loop(movement_cmd cmd) {
    return execute_command(
        "MOVE_CLOSED_LOOP",
        { { "axes", cmd.axes },
          { "speed", magic_enum::enum_name(cmd.speed) },
          { "first_axis_setpoint", cmd.first_axis_setpoint },
          { "second_axis_setpoint", cmd.second_axis_setpoint } });
}

void REMA::axes_hard_stop_all() {
    cancel_sequence_in_progress();
    execute_command("AXES_HARD_STOP_ALL");
}

void REMA::axes_soft_stop_all() {
    cancel_sequence_in_progress();
    execute_command("AXES_SOFT_STOP_ALL");
}

tl::expected<void, std::string> REMA::execute_step(movement_cmd& step) {
    nlohmann::json cmd_response = move_closed_loop(step);
    if (cmd_response["MOVE_CLOSED_LOOP"].contains("error")) {
        is_sequence_in_progress = false;
        return tl::make_unexpected(cmd_response["MOVE_CLOSED_LOOP"]["error"]);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(400)); // Wait for telemetry update...

    bool stopped_on_probe = false;
    bool stopped_on_condition = false;
    bool abort_from_rema = false;
    do {
        if (step.axes == "XY") {
            stopped_on_probe = telemetry.probe.x_y;
            stopped_on_condition = telemetry.on_condition.x_y;
        } else {
            stopped_on_probe = telemetry.probe.z;
            stopped_on_condition = telemetry.on_condition.z;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Avoid hogging the processor
        abort_from_rema = !telemetry.control_enabled || telemetry.stalled.x || telemetry.stalled.y ||
                            telemetry.stalled.z || telemetry.probe_protected;
    } while (!(stopped_on_probe || stopped_on_condition || cancel_sequence || abort_from_rema));

    if (cancel_sequence || abort_from_rema) {
        is_sequence_in_progress = false;
        return tl::make_unexpected("Sequence cancelled");            
    } else {
        step.executed = true;
        step.execution_results.coords = telemetry.coords;
        step.execution_results.stopped_on_probe = stopped_on_probe;
        step.execution_results.stopped_on_condition = stopped_on_condition;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(250)); // Wait for vibrations to stop
    return {};
}

tl::expected<void, std::string> REMA::execute_sequence(movement_cmd& step) {
    nlohmann::json res;
    cancel_sequence_in_progress();
    cancel_sequence = false;
    is_sequence_in_progress = true;
    execute_command("AXES_HARD_STOP_ALL");

    auto ret = execute_step(step);
    if (!ret) {
        return ret;
    }
    is_sequence_in_progress = false;
    cancel_sequence = false;
    return {};
}

tl::expected<void, std::string> REMA::execute_sequence(std::vector<movement_cmd>& sequence) {
    nlohmann::json res;
    cancel_sequence_in_progress();
    cancel_sequence = false;
    is_sequence_in_progress = true;
    execute_command("AXES_HARD_STOP_ALL");

    for (auto &step : sequence) {
        auto ret = execute_step(step);
        if (!ret) {
            return ret;
        }
    }
    is_sequence_in_progress = false;
    cancel_sequence = false;
    return {};
}


nlohmann::json REMA::would_move_touch_probe(std::string new_tool_string) {
    nlohmann::json res = nlohmann::json::object();
    Tool new_tool = get_tool(new_tool_string);
    if (get_selected_tool().is_touch_probe != new_tool.is_touch_probe) {
        if (!telemetry.control_enabled) {
            res["error"]="CONTROL IS DISABLED";
        } else {
            std::string message = "⚠️ WARNING: CRITICAL OPERATION ⚠️\n\n";
            if (new_tool.is_touch_probe) {
                message.append("DANGER! Executing this command will EXTEND the touch probe. \n");
            } else {
                message.append("DANGER! Executing this command will RETRACT the touch probe. \n");
            }
            message.append("Proceed with caution, as any obstruction during movement can cause irreversible damage to the probe.\n" 
                "Ensure the path is completely clear before proceeding.");
            res["message"] = message;
        }
    }    
    return res;
}

tl::expected<void, std::string> REMA::set_last_selected_tool(std::string tool) {
    std::string prev_last_selected_tool = last_selected_tool;
    if (tool != last_selected_tool) {
        if (get_selected_tool().is_touch_probe) {
            auto ret = retract_touch_probe();
            if (!ret) {
                return ret;
            }
        }

        last_selected_tool = tool;
        if (get_selected_tool().is_touch_probe) {
            auto ret = extend_touch_probe();
            if (!ret) {
                last_selected_tool = prev_last_selected_tool;
                return ret;
            }
        }
        save_config();
    }
    return {};
}

Tool REMA::get_tool(std::string tool) const {
    if (auto iter = tools.find(tool); iter != tools.end()) {
        return iter->second;
    }
    return {};
}

Tool REMA::get_selected_tool() const {
    if (auto iter = tools.find(last_selected_tool); iter != tools.end()) {
        return iter->second;
    }
    return {};
}

tl::expected<void, std::string> REMA::extend_touch_probe() {
    nlohmann::json cmd_response = execute_command("TOUCH_PROBE", {{ "position", "EXTEND" }});
    if (cmd_response["TOUCH_PROBE"].contains("error")) {
        is_sequence_in_progress = false;
        return tl::make_unexpected(cmd_response["TOUCH_PROBE"]["error"]);
    }
    return {};
};

tl::expected<void, std::string> REMA::retract_touch_probe() {
    nlohmann::json cmd_response = execute_command("TOUCH_PROBE", {{ "position", "RETRACT" }});
    if (cmd_response["TOUCH_PROBE"].contains("error")) {
        is_sequence_in_progress = false;
        return tl::make_unexpected(cmd_response["TOUCH_PROBE"]["error"]);
    }
    return {};
};