#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <csv.h>
#include <json.hpp>

#include "rema.hpp"
#include "tool.hpp"

void REMA::save_to_disk() const {
    std::ofstream file(rema_file);
    nlohmann::json j;

    j["last_selected_tool"] = last_selected_tool;
    file << j;
}

void REMA::set_selected_tool(std::filesystem::path tool) {
    last_selected_tool = tool;
    save_to_disk();

}

std::filesystem::path REMA::get_selected_tool() const {
    return last_selected_tool;
}
