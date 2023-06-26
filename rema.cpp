#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <csv.h>
#include <json.hpp>

#include "rema.hpp"
#include "tool.hpp"


REMA::REMA(std::filesystem::path rema_file) {
    std::ifstream i(rema_file);

    nlohmann::json j;
    i >> j;
    this->loaded = true;
    this->last_selected_tool = std::filesystem::path(j["last_selected_tool"]);
}

void REMA::save_to_disk() const {
    std::ofstream file(rema_file);
    file << nlohmann::json(*this);
}

void REMA::set_selected_tool(std::filesystem::path tool) {
    last_selected_tool = tool;
    changed = true;
}

std::filesystem::path REMA::get_selected_tool() const {
    return last_selected_tool;
}
