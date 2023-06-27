#ifndef TOOL_HPP
#define TOOL_HPP

#include <string>
#include <json.hpp>

class Tool {
public:
    Tool() {
    }
    ;

    Tool(std::filesystem::path tool_file);
    void save_to_disk() const;

    std::string name;
    float offset_x;
    float offset_y;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Tool, name, offset_x, offset_y)

#endif 		// TOOL_HPP
