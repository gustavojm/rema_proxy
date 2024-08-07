#include <fstream>
#include <iostream>

#include "chart.hpp"

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

void Chart::save_to_disk() {
    active_obj.send([this] {
        auto now = to_time_t(std::chrono::steady_clock::now());
        std::filesystem::path chart_file = charts_dir / ("chart" + std::to_string(now) + ".json");

        std::ofstream o_file_stream(chart_file);
        o_file_stream << make_chart_data();
        timestamps.clear();
        coords_x.clear();
        coords_y.clear();
        coords_z.clear();
    });
}

std::vector<std::string> Chart::list() {
    std::vector<std::string> res;

    for (const auto &entry : std::filesystem::directory_iterator(charts_dir)) {
        if (entry.is_regular_file()) {
            res.push_back(entry.path().filename().replace_extension());
        }
    }
    return res;
}

nlohmann::json Chart::load_from_disk(std::string file_name) {
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