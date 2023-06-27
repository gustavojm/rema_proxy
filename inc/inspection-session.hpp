#ifndef INSP_SESSION_HPP
#define INSP_SESSION_HPP

#include <string>
#include <filesystem>
#include <json.hpp>

template<typename TP>
std::time_t to_time_t(TP tp) {
    auto sctp =
            std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    tp - TP::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(sctp);
}

static inline std::filesystem::path insp_sessions_dir = std::filesystem::path(
        "insp_sessions");

struct InspectionPlanEntry {
    std::string row, col;
    bool inspected;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(InspectionPlanEntry, row, col, inspected)

class InspectionSession {
public:
    InspectionSession();

    explicit InspectionSession(std::string session_name, std::filesystem::path hx);

    explicit InspectionSession(std::filesystem::path inspection_session_file);

    std::string load_plans();

    bool load(std::string session_name);

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

                InspectionSession insp_session(entry);
                insp_session.last_write_time = buffer.str();

                res.push_back(insp_session);
            }
        }
        //std::sort(res.begin(), res.end());
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

    void set_tube_inspected(std::string tube_id, bool state);

    void set_tube_inspected(std::string insp_plan,
            std::string tube_id, bool state);

    static void delete_session(std::string session_name) {
        std::filesystem::remove(insp_sessions_dir / (session_name + std::string(".json")));
    }

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
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(InspectionSession, name,
        last_write_time, hx_directory, hx, tubesheet_csv, tubesheet_svg,
        last_selected_plan, insp_plans, leg, tube_od, unit)

#endif 		// INSP_SESSION_HPP
