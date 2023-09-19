#ifndef INSP_SESSION_HPP
#define INSP_SESSION_HPP

#include <string>
#include <filesystem>
#include <json.hpp>
#include <set>

#include "points.hpp"
#include "tool.hpp"

template<typename TP>
std::time_t to_time_t(TP tp) {
    auto sctp =
            std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    tp - TP::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(sctp);
}

static inline std::filesystem::path insp_sessions_dir = std::filesystem::path(
        "insp_sessions");

class InspectionPlanEntry {
public:
    int seq;
    std::string row, col;
    bool inspected;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(InspectionPlanEntry, seq, row, col, inspected)

class CalPointEntry {
public:
    std::string col;
    std::string row;
    Point3D ideal_coords;
    Point3D determined_coords;
    bool determined;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CalPointEntry, col, row, ideal_coords, determined_coords, determined)

class TubeEntry {
public:
    std::string x_label;
    std::string y_label;
    Point3D coords;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TubeEntry, x_label, y_label, coords)

class InspectionSession {
public:
    InspectionSession();

    explicit InspectionSession(std::string session_name, std::filesystem::path hx);

    explicit InspectionSession(std::filesystem::path inspection_session_file);

    void process_csv();

    std::string load_plans();

    std::map<std::string, struct InspectionPlanEntry> inspection_plan_get(std::string insp_plan);

    void cal_points_add_update(std::string tube_id, std::string col, std::string row, Point3D ideal_coords, Point3D determined_coords);

    void cal_points_delete(std::string id);

    Point3D get_tube_coordinates(std::string tube_id, bool ideal);

    void generate_svg();

    bool load(std::string session_name);

    Point3D from_rema_to_ui(Point3D coords, Tool* tool = nullptr) {
        if (tool) {
            return ((coords - tool->offset) * scale);
        } else {
            return (coords * scale);
        }
    }

    double from_rema_to_ui(double meassure) {
        return (meassure * scale);
    }

    Point3D from_ui_to_rema(Point3D coords, Tool* tool = nullptr) {
        if (tool) {
            return ((coords / scale) + tool->offset);
        } else {
            return (coords / scale);
        }
    }

    double from_ui_to_rema(double meassure) {
        return (meassure / scale);
    }

    static std::vector<InspectionSession> sessions_list() {
        std::vector<InspectionSession> res;

        for (const auto &entry : std::filesystem::directory_iterator(
                insp_sessions_dir)) {
            if (entry.is_regular_file()) {
                std::time_t tt = to_time_t(entry.last_write_time());
                std::tm *gmt = std::gmtime(&tt);
                std::stringstream buffer;
                buffer << std::put_time(gmt, "%A, %d %B %Y %H:%M");
                std::string formattedFileTime = buffer.str();

                InspectionSession insp_session(entry.path().filename().replace_extension());
                insp_session.last_write_time = buffer.str();

                res.push_back(insp_session);
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

    inline void set_changed(bool changed) {
        this->changed = changed;
    }

    void set_selected_plan(std::string plan);

    std::string get_selected_plan() const;

    void set_tube_inspected(std::string insp_plan,
            std::string tube_id, bool state);

    static void delete_session(std::string session_name) {
        std::filesystem::remove(insp_sessions_dir / (session_name + std::string(".json")));
    }

    std::map<std::string, Point3D>& calculate_aligned_tubes();

    std::map<std::string,
            std::map<std::string,
                    struct InspectionPlanEntry>> insp_plans;

    std::string name;
    std::filesystem::path hx_directory;
    std::filesystem::path hx;
    std::filesystem::path tubesheet_csv;
    std::filesystem::path tubesheet_svg;
    std::string last_selected_plan;
    std::string last_write_time;
    float tube_od;
    std::string leg = "both";
    bool loaded = false;
    bool changed = false;
    std::string unit = "inch";
    double scale = 1;
    std::map<std::string, TubeEntry> tubes;
    std::map<std::string, Point3D> aligned_tubes;
    struct {
        std::vector<std::string> config_x_labels_coords;
        std::vector<std::string> config_y_labels_coords;
        std::set<std::pair<std::string, float>> x_labels;
        std::set<std::pair<std::string, float>> y_labels;
    } svg;
    std::map<std::string, CalPointEntry> cal_points;
    int total_tubes_in_plans = 0;
    int total_tubes_inspected = 0;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(InspectionSession, name,
        last_write_time, hx_directory, hx, tubesheet_csv, tubesheet_svg,
        last_selected_plan, insp_plans, leg, tube_od, unit, scale, cal_points, total_tubes_in_plans, total_tubes_inspected)

#endif 		// INSP_SESSION_HPP
