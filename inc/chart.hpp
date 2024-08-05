#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include "telemetry.hpp"
#include "points.hpp"

inline std::filesystem::path charts_dir = "charts";

template<typename TP> std::time_t to_time_t(TP tp) {
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        tp - TP::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(sctp);
}

struct ChartEntry {
    Point3D coords;
    Point3D targets;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ChartEntry, coords, targets)

class Chart {

public:
    Chart() noexcept = default;

    Chart(const std::filesystem::path &chart_file);

    void insertData(const ChartEntry& data);
    
    void save_to_disk() const;

    nlohmann::json queryData(std::time_t startTime, std::time_t endTime);

    std::map<std::time_t, ChartEntry> chart_data;
    std::filesystem::path db_name;
};


