#ifndef TOOL_HPP
#define TOOL_HPP

#include <string>
#include <json.hpp>

class Tool {
public:
    Tool() {
    }

    Tool(std::string name, float offset_x, float offset_y, float offset_z) :
            name(name), offset_x(offset_x), offset_y(offset_y), offset_z(
                    offset_z) {
        save_to_disk();
    }

    Tool(std::filesystem::path tool_file);

    void save_to_disk() const;

    std::string name;
    float offset_x = 0;
    float offset_y = 0;
    float offset_z = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Tool, name, offset_x, offset_y,
        offset_z)

#endif 		// TOOL_HPP
