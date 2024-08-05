#include <iostream>
#include <fstream>

#include "chart.hpp"

Chart::Chart(const std::filesystem::path &chart_file) : db_name(chart_file) {
    db_name.replace_extension(".json");
    std::ifstream i_file_stream(charts_dir / db_name);
    std::cout << "opening: "<< (charts_dir / db_name) << std::endl;

    nlohmann::json json;
    try {
        i_file_stream >> json;
        chart_data = json;
    } catch (std::exception &e) {
        SPDLOG_ERROR("{}", e.what());
    }
}

void Chart::insertData(const ChartEntry& data) {
    auto timestamp = to_time_t(std::chrono::steady_clock::now());
    chart_data.insert({timestamp, data});
    save_to_disk();
}

void Chart::save_to_disk() const {
    std::filesystem::path chart_file = charts_dir / db_name;

    std::ofstream o_file_stream(chart_file);
    nlohmann::json json(chart_data);
    o_file_stream << json;
}

nlohmann::json Chart::queryData(std::time_t startTime, std::time_t endTime) {    
    nlohmann::json result;

    std::for_each(chart_data.begin(), chart_data.end(),
        [startTime, endTime, &result] (auto it) {
            if (it.first >= startTime && it.first <= endTime) {
                result.push_back({it.first, it.second});
            }
        }
    );

    return result;   
}