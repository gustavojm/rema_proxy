#pragma once

#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>

#include "active.hpp"
#include "points.hpp"
#include "telemetry.hpp"
#include "time_utils.hpp"

inline std::filesystem::path charts_dir = "charts";

class Chart {

  public:
    Chart() noexcept;

    void insertData(const Point3D &coords);

    void init(std::string type);

    nlohmann::json load_from_disk(std::string file_name);

    static std::vector<std::string> list();

    static void delete_chart(const std::string &chart_file);

    nlohmann::json make_chart_data() const;
  
    void close_curent();

    std::ofstream chart_file_stream;

    std::vector<std::time_t> timestamps;
    std::vector<double> coords_x;
    std::vector<double> coords_y;
    std::vector<double> coords_z;

    Active active_obj;
};

inline Chart chart;
