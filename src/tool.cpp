#include <csv.h>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include "rema.hpp"
#include "tool.hpp"

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
