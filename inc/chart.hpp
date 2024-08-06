#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include "telemetry.hpp"
#include "points.hpp"
#include "active.hpp"
#include "time_utils.hpp"

inline std::filesystem::path charts_dir = "charts";

class Chart {

public:
    Chart() noexcept = default;

    void insertData(const Point3D& coords);
    
    void save_to_disk();

    nlohmann::json load_from_disk(std::string file_name);

    static std::vector<std::string> list();

    static void delete_chart(const std::string &chart_file);

    nlohmann::json make_chart_js_data() const;

    std::vector<std::time_t> timestamps;
    std::vector<double> speed_x;
    std::vector<double> speed_y;
    std::vector<double> speed_z;
    std::chrono::time_point<std::chrono::system_clock> prev_timestamp;
    Point3D prev_coords = {};

    Active active_obj;

};

inline Chart chart;
