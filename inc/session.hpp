#pragma once

#include <filesystem>
#include <set>
#include <string>

#include "nlohmann/json.hpp"
#include "HX.hpp"
#include "points.hpp"
#include "tool.hpp"
#include "tube_entry.hpp"
#include "misc_fns.hpp"

inline std::filesystem::path sessions_dir = std::filesystem::path("sessions");

class PlanEntry {
  public:
    int seq;
    std::string row;
    std::string col;
    bool executed;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlanEntry, seq, row, col, executed)

class CalPointEntry {
  public:
    std::string col;
    std::string row;
    Point3D ideal_coords;
    Point3D determined_coords;
    bool determined;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CalPointEntry, col, row, ideal_coords, determined_coords, determined)

class Session {
  public:
    Session() noexcept;

    explicit Session(const std::string& session_name, const std::filesystem::path& hx_dir_);

    explicit Session(const std::filesystem::path& session_file);

    void load_plan_from_disk(const std::filesystem::path& plan_file);

    void load_plan(const std::string& plan_name, std::istream& stream);

    std::string load_plans();

    std::map<std::string, struct PlanEntry> plan_get(const std::string& plan);

    void plan_remove(const std::string& plan);

    void cal_points_add_update(
        const std::string& tube_id,
        const std::string& col,
        const std::string& row,
        Point3D& ideal_coords,
        Point3D& determined_coords);

    void cal_points_delete(const std::string& id);

    Point3D get_tube_coordinates(const std::string& tube_id, bool ideal);

    bool load(const std::string& session_name);

    Point3D from_rema_to_ui(Point3D coords, Tool* tool = nullptr);

    double from_rema_to_ui(double meassure);

    Point3D from_ui_to_rema(Point3D coords, Tool* tool = nullptr);

    double from_ui_to_rema(double meassure);

    static std::vector<Session> sessions_list() ;

    void save_to_disk() const;

    void set_selected_plan(std::string& plan);

    std::string get_selected_plan() const;

    void set_tube_executed(std::string& plan, std::string& tube_id, bool state);

    int total_tubes_in_plans();

    int total_tubes_executed();

    static void delete_session(std::string session_name);

    void copy_tubes_to_aligned_tubes();

    HX calculate_aligned_HX();

    std::map<std::string, std::map<std::string, struct PlanEntry>> plans;

    nlohmann::json to_json_to_disk() const;

    void from_json_from_disk(const nlohmann::json& json);

    std::string name;
    std::filesystem::path hx_dir;
    HX hx;
    HX aligned_hx;
    std::string last_selected_plan;
    std::string last_write_time;
    bool is_loaded = false;
    bool is_changed = false;
    bool is_aligned = false;
    std::map<std::string, CalPointEntry> cal_points;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    Session,
    name,
    last_write_time,
    hx_dir,
    hx,
    aligned_hx,
    last_selected_plan,
    plans,
    cal_points,    
    is_aligned,
    is_loaded)

inline Session current_session;