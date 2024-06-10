#ifndef REMA_HPP
#define REMA_HPP

#include <filesystem>
#include <json.hpp>
#include <mutex>
#include <telemetry_net_client.hpp>
#include <command_net_client.hpp>
#include <string>

#include "points.hpp"
#include "session.hpp"
#include "tool.hpp"

extern const std::filesystem::path config_file_path;
extern const std::filesystem::path rema_dir;
extern const std::filesystem::path tools_dir;

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
    struct compound_axes on_condition;
    struct compound_axes probe;
    struct individual_axes stalled;
    struct limits limits;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(telemetry, coords, on_condition, probe, stalled, limits)

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
    static REMA &get_instance() {
        static REMA instance; // Guaranteed to be destroyed. Instantiated on first use.
        return instance;
    }

    static void add_tool(const Tool &tool) {
        tools[tool.name] = tool;
    }

    static void delete_tool(const std::string &tool) {
        std::filesystem::remove(tools_dir / (tool + std::string(".json")));
        tools.erase(tool);
    }

    void connect(const std::string &rtu_host, int rtu_port);

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
        execute_command({ { "command", "TOUCH_PROBE" },
                          { "pars",
                            {
                                { "position", "IN" },
                            } } });
    };

    void retract_touch_probe() {
        execute_command({ { "command", "TOUCH_PROBE" },
                          { "pars",
                            {
                                { "position", "OUT" },
                            } } });
    };
        
    void load_config();

    void save_config();

    void execute_command(const nlohmann::json command);

    void execute_command_no_wait(const nlohmann::json command);

    void move_closed_loop(movement_cmd cmd);

    void axes_hard_stop_all();

    void axes_soft_stop_all();

    void cancel_sequence_in_progress();

    bool execute_sequence(std::vector<movement_cmd> &sequence);

    void set_home_xy(double x, double y);

    void set_home_z(double z);

    bool loaded = false;

    static std::map<std::string, Tool> tools;

    std::string last_selected_tool;

    CommandNetClient command_client;
    TelemetryNetClient telemetry_client;

    bool is_sequence_in_progress;
    bool cancel_sequence;
    nlohmann::json config;

    // Telemetry values
    std::mutex mtx;
    struct telemetry telemetry;
    struct temps temps;

  private:
    REMA() :
        telemetry_client([&](std::string line){update_telemetry(line);})
     {
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

#endif // REMA_HPP
