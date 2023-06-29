#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <csv.h>

#include "rema.hpp"
#include "tool.hpp"

extern std::filesystem::path tools_dir;

Tool::Tool(std::filesystem::path tool_file) {
    std::ifstream i(tool_file);

    nlohmann::json j;
    i >> j;
    *this = j;
    this->name = tool_file.filename().replace_extension();
}

void Tool::save_to_disk() const {
    std::filesystem::path tool_file = tools_dir / (name + std::string(".json"));

    std::ofstream file(tool_file);
    nlohmann::json j(*this);
    j.erase("name");
    file << j;
}
