#ifndef TUBE_ENTRY_HPP
#define TUBE_ENTRY_HPP

#include <string>

#include "points.hpp"
#include "json.hpp"

class TubeEntry {
public:
    std::string x_label;
    std::string y_label;
    Point3D coords;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TubeEntry, x_label, y_label, coords)

#endif // TUBE_ENTRY_HPP
