#include <csv.hpp>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include "rema.hpp"
#include "tool.hpp"

Tool::Tool(std::string tool_name, Point3D offset_, bool is_touch_probe_) : name(tool_name), is_touch_probe(is_touch_probe_) {
    if (is_touch_probe) {
        offset = {};
    } else {
        offset = offset_;
    }
}

Tool::Tool(const std::filesystem::path &tool_file) {
    std::ifstream i_file_stream(tool_file);

    nlohmann::json json;
    i_file_stream >> json;
    *this = json;
    this->name = tool_file.filename().replace_extension();
}

void Tool::save_to_disk() const {
    std::filesystem::path tool_file = tools_dir / (name + std::string(".json"));

    std::ofstream o_file_stream(tool_file);
    nlohmann::json json(*this);
    json.erase("name");
    o_file_stream << json;
}
