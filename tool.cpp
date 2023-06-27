#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <csv.h>

#include "tool.hpp"

    Tool::Tool(std::filesystem::path tool_file) {
        std::ifstream i(tool_file);

        nlohmann::json j;
        i >> j;
        *this = j;
        this->name = tool_file.filename().replace_extension();
    }


void Tool::save_to_disk() const {
    std::filesystem::path tool_file = std::filesystem::path(name).append(".json");
    std::ofstream file(tool_file);
    nlohmann::json j(*this);
    j.erase("name");
    file << j;
}
