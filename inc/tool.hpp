#pragma once

#include <string>

#include "nlohmann/json.hpp"
#include "points.hpp"

class Tool {
  public:
    Tool() {
    }

    Tool(std::string tool_name, Point3D offset_, bool is_touch_probe_);

    Tool(const std::filesystem::path &tool_file);

    void save_to_disk() const;

    std::string name;
    Point3D offset;
    bool is_touch_probe = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Tool, name, offset, is_touch_probe)
