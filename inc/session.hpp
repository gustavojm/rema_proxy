#ifndef SESSION_HPP
#define SESSION_HPP

#include <filesystem>
#include <json.hpp>
#include <set>
#include <string>

#include "HX.hpp"
#include "points.hpp"
#include "tool.hpp"
#include "tube_entry.hpp"

template<typename TP> std::time_t to_time_t(TP tp) {
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        tp - TP::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(sctp);
}

static inline std::filesystem::path sessions_dir = std::filesystem::path("sessions");

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

    Point3D from_rema_to_ui(Point3D coords, Tool *tool = nullptr) {
        if (tool) {
            return ((coords - tool->offset) * hx.scale);
        } else {
            return (coords * hx.scale);
        }
    }

    double from_rema_to_ui(double meassure) {
        return (meassure * hx.scale);
    }

    Point3D from_ui_to_rema(Point3D coords, Tool *tool = nullptr) {
        if (tool) {
            return ((coords / hx.scale) + tool->offset);
        } else {
            return (coords / hx.scale);
        }
    }

    double from_ui_to_rema(double meassure) {
        return (meassure / hx.scale);
    }

    static std::vector<Session> sessions_list() {
        std::vector<Session> res;

        for (const auto& entry : std::filesystem::directory_iterator(sessions_dir)) {
            if (entry.is_regular_file()) {
                std::time_t tt = to_time_t(entry.last_write_time());
                std::tm *gmt = std::gmtime(&tt);
                std::stringstream buffer;
                buffer << std::put_time(gmt, "%A, %d %B %Y %H:%M");
                std::string formattedFileTime = buffer.str();

                Session session(entry.path().filename().replace_extension());
                session.last_write_time = buffer.str();

                res.push_back(session);
            }
        }
        return res;
    }

    void save_to_disk() const;

    inline bool is_loaded() {
        return loaded;
    }

    inline bool is_changed() const {
        return changed;
    }

    inline void set_changed(bool status) {
        changed = status;
    }

    void set_selected_plan(std::string& plan);

    std::string get_selected_plan() const;

    void set_tube_executed(std::string& plan, std::string& tube_id, bool state);

    int total_tubes_in_plans();

    int total_tubes_executed();

    static void delete_session(std::string session_name) {
        std::filesystem::remove(sessions_dir / (session_name + std::string(".json")));
    }

    void copy_tubes_to_aligned_tubes();

    std::map<std::string, Point3D>& calculate_aligned_tubes();

    std::map<std::string, std::map<std::string, struct PlanEntry>> plans;

    std::string name;
    std::filesystem::path hx_dir;
    HX hx;
    std::string last_selected_plan;
    std::string last_write_time;
    bool loaded = false;
    bool changed = false;
    std::map<std::string, Point3D> aligned_tubes;
    std::map<std::string, CalPointEntry> cal_points;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    Session,
    name,
    last_write_time,
    hx_dir,
    hx,
    last_selected_plan,
    plans,
    cal_points)

#endif // SESSION_HPP
