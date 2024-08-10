#pragma once

#include <filesystem>
#include <mutex>
#include <string>

#include "command_net_client.hpp"
#include "nlohmann/json.hpp"
#include "telemetry_net_client.hpp"
#include "logs_net_client.hpp"
#include "tl/expected.hpp"
#include "points.hpp"
#include "session.hpp"
#include "tool.hpp"
#include "telemetry.hpp"
#include "log_pattern.hpp"

inline const std::filesystem::path config_file_path = "config.json";
inline const std::filesystem::path rema_dir = std::filesystem::path("rema");
inline const std::filesystem::path tools_dir = rema_dir / "tools";
inline const std::filesystem::path logs_dir = "logs";

struct temps {
    double x, y, z;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(temps, x, y, z)

struct movement_cmd {
    std::string axes;
    double first_axis_setpoint;
    double second_axis_setpoint;
    bool executed = false;
    struct {
        Point3D coords;
        bool stopped_on_probe;
        bool stopped_on_condition;
    } execution_results;
};

class REMA {
  public:
    static void add_tool(const Tool &tool) {
        tools[tool.name] = tool;
    }

    static void delete_tool(const std::string &tool) {
        std::filesystem::remove(tools_dir / (tool + std::string(".json")));
        tools.erase(tool);
    }

    void connect(const std::string &rtu_host, int rtu_port);

    void reconnect();

    void update_telemetry(std::vector<uint8_t>& stream);
    
    void save_logs(std::string &stream);
    
    void set_last_selected_tool(std::string tool) {
        if (get_selected_tool().is_touch_probe) {
            retract_touch_probe();
        }
        last_selected_tool = tool;
        if (get_selected_tool().is_touch_probe) {
            extend_touch_probe();
        }
        save_config();
    }

    Tool get_selected_tool() const {
        if (auto iter = tools.find(last_selected_tool); iter != tools.end()) {
            return iter->second;
        }
        return {};
    }

    void extend_touch_probe() {
        execute_command(
            "TOUCH_PROBE",
            {
                { "position", "IN" },
            });
    };

    void retract_touch_probe() {
        execute_command(
            "TOUCH_PROBE",
            {
                { "position", "OUT" },
            });
    };

    void load_config();

    void save_config();

    nlohmann::json send_startup_commands();

    void execute_command_no_wait(const std::string cmd_name, const nlohmann::json command);

    nlohmann::json execute_command(const std::string cmd_name, const nlohmann::json pars);

    nlohmann::json move_closed_loop(movement_cmd cmd);

    void axes_hard_stop_all();

    void axes_soft_stop_all();

    void cancel_sequence_in_progress();

    tl::expected<void, std::string> execute_sequence(std::vector<movement_cmd> &sequence);

    void set_home_xy(double x, double y);

    void set_home_z(double z);

    bool loaded = false;

    static std::map<std::string, Tool> tools;

    std::string last_selected_tool;

    CommandNetClient command_client;
    TelemetryNetClient telemetry_client;
    LogsNetClient logs_client;

    volatile bool is_sequence_in_progress;
    volatile bool cancel_sequence;
    nlohmann::json config;

    // Telemetry values
    std::mutex mtx;
    struct telemetry telemetry;
    struct telemetry ui_telemetry;
    struct telemetry old_telemetry;
    struct temps temps;

    std::vector<std::string> logs_vector;
    std::ofstream logs_ofstream;

    std::string rtu_host_;
    int rtu_port_;

    REMA() {

        spdlog::set_pattern(log_pattern);

        telemetry_client.set_on_receive_callback(
            [&](std::vector<uint8_t> line) { 
                update_telemetry(line);
            }
        );

        logs_client.set_on_receive_callback(
            [&](std::string line) { 
                save_logs(line); 
            }
        );

        auto now = to_time_t(std::chrono::steady_clock::now());
        std::filesystem::path log_file = logs_dir / ("log" + std::to_string(now) + ".json");
       
        try {           
            if (!std::filesystem::exists(logs_dir)) {
                std::filesystem::create_directories(logs_dir);
            }

            logs_ofstream.open(log_file, std::fstream::out | std::ios::app);
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

    // C++ 11
    // =======
    // We can use the better technique of deleting the methods
    // we don't want.

  public:
    REMA(REMA const &) = delete;
    REMA &operator=(REMA const &) = delete;

    // Note: Scott Meyers mentions in his Effective Modern
    //       C++ book, that deleted functions should generally
    //       be public as it results in better error messages
    //       due to the compilers behavior to check accessibility
    //       before deleted status
};

inline std::map<std::string, Tool> REMA::tools;
inline REMA rema;
