#include <iostream>
#include <fstream>

#include "chart.hpp"

void Chart::insertData(const Point3D& coords) {
    active_obj.Send([&coords, this] {
        auto timestamp = std::chrono::system_clock::now();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch());

        timestamps.push_back(millis.count());
        double time_dif = timeDifference(timestamp, prev_timestamp);
        speed_x.push_back((prev_coords.x - coords.x) / time_dif);
        speed_y.push_back((prev_coords.y - coords.y) / time_dif);
        speed_z.push_back((prev_coords.z - coords.z) / time_dif);
        
        prev_timestamp = timestamp;
        prev_coords = coords;
    }); 
}

void Chart::save_to_disk() {
    active_obj.Send([this] {
        auto now = to_time_t(std::chrono::steady_clock::now());
        std::filesystem::path chart_file = charts_dir / ("chart" + std::to_string(now) + ".json");
        
        std::ofstream o_file_stream(chart_file);
        o_file_stream << make_chart_js_data();
        timestamps.clear();
        speed_x.clear();
        speed_y.clear();
        speed_z.clear();
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


nlohmann::json Chart::make_chart_js_data() const {
    nlohmann::json chart_json;   
    chart_json["labels"] = timestamps;

    nlohmann::json ds_x;
    ds_x["label"] = "X";
    ds_x["data"] = speed_x;
    ds_x["fill"] = false;    
    chart_json["datasets"].push_back(ds_x);

    nlohmann::json ds_y;
    ds_y["label"] = "Y";
    ds_y["data"] = speed_y;
    ds_y["fill"] = false;    
    chart_json["datasets"].push_back(ds_y);

    nlohmann::json ds_z;
    ds_z["label"] = "Z";
    ds_z["data"] = speed_z;
    ds_z["fill"] = false;    
    chart_json["datasets"].push_back(ds_z);

    return chart_json;
}