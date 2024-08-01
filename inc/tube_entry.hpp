#pragma once

#include <string>

#include "json.hpp"
#include "points.hpp"

class TubeEntry {
  public:
    std::string x_label;
    std::string y_label;
    Point3D coords;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TubeEntry, x_label, y_label, coords)
