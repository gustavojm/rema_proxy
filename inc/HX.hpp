#ifndef HX_HPP
#define HX_HPP

#include <filesystem>
#include <vector>
#include <set>
#include <iostream>

#include "csv.h"
#include "svg.hpp"
#include "points.hpp"
#include "json.hpp"
#include "tube_entry.hpp"

class HX {
public:
    static const std::filesystem::path hxs_path;

    void process_csv_from_disk(std::string hx_name);

    void process_csv(std::string hx_name, std::istream &stream);

    void generate_svg(std::string hx);

    void load_config(std::string hx);

    static std::vector<std::string> list();

    static bool create(std::string hx_name, std::string tubesheet_csv, std::string config_json);

    static bool erase(std::string hx_name);

    std::string tubesheet_svg ;
    float tube_od;
    std::string leg = "both";
    std::string unit = "inch";
    double scale = 1;
    std::map<std::string, TubeEntry> tubes;
    struct {
        std::vector<std::string> config_x_labels_coords;
        std::vector<std::string> config_y_labels_coords;
        std::set<std::pair<std::string, float>> x_labels;
        std::set<std::pair<std::string, float>> y_labels;
    } svg;

};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(HX, tubesheet_svg, tube_od,
        leg, unit, scale, tubes)

#endif // HX_HPP
