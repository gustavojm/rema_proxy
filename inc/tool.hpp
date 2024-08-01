#pragma once

#include <json.hpp>
#include <string>

#include "points.hpp"

class Tool {
  public:
    Tool() {
    }

    Tool(std::string tool_name, Point3D offset_, bool is_touch_probe_) : name(tool_name), is_touch_probe(is_touch_probe_) {

        if (is_touch_probe) {
            offset = {};
        } else {
            offset = offset_;
        }
    }

    Tool(const std::filesystem::path &tool_file);

    void save_to_disk() const;

    std::string name;
    Point3D offset;
    bool is_touch_probe = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Tool, name, offset, is_touch_probe)
