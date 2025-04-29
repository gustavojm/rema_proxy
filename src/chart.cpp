#include <fstream>
#include <iostream>

#include "chart.hpp"
#include "log_pattern.hpp"

Chart::Chart() noexcept {
    spdlog::set_pattern(log_pattern);

    try {           
    if (!std::filesystem::exists(charts_dir)) {
        std::filesystem::create_directories(charts_dir);
    }
    SPDLOG_INFO("Generating charts to ./{}/", charts_dir.string());
    } catch (std::exception& e) {
        SPDLOG_INFO(e.what());
    }
}

void Chart::insertData(const Point3D &coords) {
    active_obj.send([&coords, this] {
        auto timestamp = std::chrono::system_clock::now();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch());

        timestamps.push_back(millis.count());
        coords_x.push_back(coords.x);
        coords_y.push_back(coords.y);
        coords_z.push_back(coords.z);
    });
}

void Chart::close_curent() {
    if (chart_file_stream.is_open()) {
        chart_file_stream << make_chart_data();
        chart_file_stream.close();

        timestamps.clear();
        coords_x.clear();
        coords_y.clear();
        coords_z.clear();
    }
}
    
void Chart::init(std::string type) {
    active_obj.send([this, type] {
        close_curent();

        auto now = to_time_t(std::chrono::steady_clock::now());
        std::filesystem::path chart_file = charts_dir / ("chart_" + type + "_" + std::to_string(now) + ".json");
       
        try {
            chart_file_stream.open(chart_file, std::ios::app);            
        } catch (std::exception &e) {
            SPDLOG_ERROR("CHARTS STORAGE ERROR {}", e.what());
        }
    });
}

std::vector<std::string> Chart::list() {
    std::vector<std::string> res;

    std::vector<std::pair<std::filesystem::file_time_type, std::filesystem::path>> entries;
    for (const auto& entry : std::filesystem::directory_iterator(charts_dir)) {
        if (entry.is_regular_file()) {
            entries.push_back({entry.last_write_time(), entry.path()});
        }
    }

    std::sort(entries.begin(), entries.end(), std::greater<>());

    for (const auto& entry : entries) {
        res.push_back(entry.second.filename().replace_extension());
    }

    return res;
}

nlohmann::json Chart::load_from_disk(std::string file_name) {
    close_curent();
    std::filesystem::path chart_file_path = charts_dir / (file_name + std::string(".json"));
    std::ifstream i_file_stream(chart_file_path);

    nlohmann::json json;
    i_file_stream >> json;
    return json;
}

void Chart::delete_chart(const std::string &chart_file) {
    std::filesystem::remove(charts_dir / (chart_file + std::string(".json")));
}

nlohmann::json Chart::make_chart_data() const {
    nlohmann::json chart_json;
    chart_json["times"] = timestamps;
    chart_json["coords_x"] = coords_x;
    chart_json["coords_y"] = coords_y;
    chart_json["coords_z"] = coords_z;
    return chart_json;
}