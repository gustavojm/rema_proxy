#pragma once

#include <nlohmann/json.hpp>

#include <points.hpp>

struct individual_axes {
    bool x;
    bool y;
    bool z;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(individual_axes, x, y, z)

struct limits {
    bool left;
    bool right;
    bool up;
    bool down;
    bool in;
    bool out;
    bool probe;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(limits, left, right, up, down, in, out, probe)

struct compound_axes {
    bool x_y = false;
    bool z = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(compound_axes, x_y, z)

struct telemetry {
    struct Point3D coords;
    struct Point3D targets;
    struct compound_axes on_condition;
    struct compound_axes probe;
    struct individual_axes stalled;
    struct limits limits;
    bool control_enabled;
    bool stall_control;
    int brakes_mode;
    bool probe_protected;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    telemetry,
    coords,
    targets,
    on_condition,
    probe,
    stalled,
    limits,
    control_enabled,
    stall_control,
    brakes_mode,
    probe_protected)
