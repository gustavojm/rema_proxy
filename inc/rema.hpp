#pragma once

#include <filesystem>
#include <mutex>
#include <string>

#include "command_net_client.hpp"
#include "nlohmann/json.hpp"
#include "telemetry_net_client.hpp"
#include "tl/expected.hpp"
#include "points.hpp"
#include "session.hpp"
#include "tool.hpp"

inline const std::filesystem::path config_file_path = "config.json";
inline const std::filesystem::path rema_dir = std::filesystem::path("rema");
inline const std::filesystem::path tools_dir = rema_dir / "tools";

struct temps {
    double x, y, z;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(temps, x, y, z)

struct individual_axes {
    bool x;
    bool y;
    bool z;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(individual_axes, x, y, z)

struct limits {
    bool left;
    bool right;
    bool up;
    bool down;
    bool in;
    bool out;
    bool probe;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(limits, left, right, up, down, in, out, probe)

struct compound_axes {
    bool x_y = false;
    bool z = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(compound_axes, x_y, z)

struct telemetry {
    struct Point3D coords;
    struct Point3D targets;
    struct compound_axes on_condition;
    struct compound_axes probe;
    struct individual_axes stalled;
    struct limits limits;
    bool control_enabled;
    bool stall_control;
    int brakes_mode;
    bool probe_protected;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    telemetry,
    coords,
    targets,
    on_condition,
    probe,
    stalled,
    limits,
    control_enabled,
    stall_control,
    brakes_mode,
    probe_protected)

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

    void update_telemetry(std::string &stream);

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

    void send_startup_commands();

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

    volatile bool is_sequence_in_progress;
    volatile bool cancel_sequence;
    nlohmann::json config;

    // Telemetry values
    std::mutex mtx;
    struct telemetry telemetry;
    struct temps temps;

    std::string rtu_host_;
    int rtu_port_;

    REMA() : telemetry_client([&](std::string line) { update_telemetry(line); }) {
        try {
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
