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
#include "magic_enum/magic_enum.hpp"

inline const std::filesystem::path config_file_path = "config.json";
inline const std::filesystem::path rema_dir = std::filesystem::path("rema");
inline const std::filesystem::path tools_dir = rema_dir / "tools";
inline const std::filesystem::path logs_dir = "logs";

struct temps {
    double x, y, z;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(temps, x, y, z)

enum speed {SLOW, NORMAL};

struct movement_cmd {
    std::string axes;
    enum speed speed = speed::NORMAL;
    double first_axis_setpoint;
    double second_axis_setpoint;
    bool is_relevant = false;
    bool executed = false;
    struct {
        Point3D coords;
        bool stopped_on_probe;
        bool stopped_on_condition;
    } execution_results;
};

class REMA {
  public:
    static void add_tool(const Tool &tool);

    static void delete_tool(const std::string &tool);

    void connect(const std::string &rtu_host, int rtu_port);

    void reconnect();

    void update_telemetry(std::vector<uint8_t>& stream);
    
    void save_logs(std::string &stream);
    
    tl::expected<void, std::string> set_last_selected_tool(std::string tool);

    Tool get_selected_tool() const;
    
    Tool get_tool(std::string tool) const;

    nlohmann::json would_move_touch_probe(std::string new_tool_string);

    tl::expected<void, std::string> extend_touch_probe();

    tl::expected<void, std::string> retract_touch_probe();

    void load_config();

    void save_config();

    nlohmann::json send_startup_commands();

    void execute_command_no_wait(const std::string cmd_name, const nlohmann::json command);

    nlohmann::json execute_command(const std::string cmd_name, const nlohmann::json pars = {});

    nlohmann::json move_closed_loop(movement_cmd cmd);

    void axes_hard_stop_all();

    void axes_soft_stop_all();

    void cancel_sequence_in_progress();

    tl::expected<void, std::string> execute_step(movement_cmd& step);

    tl::expected<void, std::string> execute_sequence(movement_cmd& step);

    tl::expected<void, std::string> execute_sequence(std::vector<movement_cmd>& sequence);

    void set_home_xyz(Point3D coords);

    void set_home_xy(double x, double y);

    void set_home_z(double z);


    REMA();

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
    volatile bool new_temps_available;

    std::vector<std::string> logs_vector;
    std::ofstream logs_ofstream;
    std::string rtu_host_;
    int rtu_port_;
    std::mutex rtu_mutex;
};

inline std::map<std::string, Tool> REMA::tools;
inline REMA rema;
