#ifndef REMA_HPP
#define REMA_HPP

#include <open3d/Open3D.h>

#include <string>
#include <mutex>
#include <filesystem>
#include <json.hpp>
#include <net_client.hpp>

#include "tool.hpp"
#include "points.hpp"
#include "inspection-session.hpp"

static inline std::filesystem::path rema_dir = std::filesystem::path("rema");
static inline std::filesystem::path rema_file = rema_dir / "rema.json";
static inline std::filesystem::path tools_dir = rema_dir / "tools";

struct temps {
    double x, y, z;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(temps, x, y, z)

struct stalled {
    bool x;
    bool y;
    bool z;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(stalled, x, y, z)

struct limits {
    bool left;
    bool right;
    bool up;
    bool down;
    bool in;
    bool out;
    bool probe;
    bool debounced_probe;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(limits, left, right, up, down, in, out,
        probe)

struct on_condition {
    bool x_y = false;
    bool z = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(on_condition, x_y, z)

struct telemetry {
    struct Point3D coords;
    struct on_condition on_condition;
    struct stalled stalled;
    struct limits limits;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(telemetry, coords, on_condition, stalled,
        limits)

struct movement_cmd {
    std::string axes;
    double first_axis_setpoint;
    double second_axis_setpoint;
    bool stop_on_probe = true;
    bool stop_on_condition = true;
    bool executed = false;
    struct {
        Point3D coords;
        bool stopped_on_probe;
        bool stopped_on_condition;
    } execution_results;
};

class REMA {
public:
    static REMA& get_instance() {
        static REMA instance(rema_file);    // Guaranteed to be destroyed.
                                            // Instantiated on first use.
        return instance;
    }

    static void add_tool(Tool tool) {
        tools[tool.name] = tool;
    }

    static void delete_tool(std::string tool) {
        std::filesystem::remove(tools_dir / (tool + std::string(".json")));
        tools.erase(tool);
    }

    void update_telemetry(std::string &stream);

    std::map<std::string, Point3D> calculate_aligned_tubes(InspectionSession& insp_sess, std::vector<Point3D> src_points, std::vector<Point3D> dst_points);

    void set_last_selected_tool(std::string tool) {
        last_selected_tool = tool;
        save_to_disk();
    }

    Tool get_selected_tool() const {
        return tools.at(last_selected_tool);
    }

    void save_to_disk() const;

    void execute_command(nlohmann::json command);

    void move_closed_loop(movement_cmd step);

    void axes_hard_stop_all();

    void axes_soft_stop_all();

    void cancel_sequence_in_progress();

    bool execute_sequence(std::vector<movement_cmd>& sequence);

    void set_home_xy(double x, double y);

    void set_home_z(double x);

    bool loaded = false;

    static std::map<std::string, Tool> tools;

    std::string last_selected_tool;

    netClient command_client;
    netClient telemetry_client;

    bool is_sequence_in_progress;
    bool cancel_sequence;

    // Telemetry values
    std::mutex mtx;
    struct telemetry telemetry;
    struct temps temps;

private:
    REMA(std::filesystem::path path) {
        std::ifstream i(path);
        nlohmann::json j;
        i >> j;
        this->last_selected_tool = j["last_selected_tool"];

        for (const auto &entry : std::filesystem::directory_iterator(tools_dir)) {
            Tool t(entry.path());
            tools[entry.path().filename().replace_extension()] = t;
        }
        this->loaded = true;
    }

    // C++ 11
    // =======
    // We can use the better technique of deleting the methods
    // we don't want.

public:
    REMA(REMA const&) = delete;
    REMA& operator=(REMA const&) = delete;

    // Note: Scott Meyers mentions in his Effective Modern
    //       C++ book, that deleted functions should generally
    //       be public as it results in better error messages
    //       due to the compilers behavior to check accessibility
    //       before deleted status

};

#endif 		// REMA_HPP
