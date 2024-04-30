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

    static std::vector<std::string> list() {
        std::vector<std::string> res;
        for (const auto &entry : std::filesystem::directory_iterator(hxs_path)) {
            if (entry.is_directory()) {
                res.push_back(entry.path().filename());
            }
        }
        return res;
    }

    static bool create(std::string hx_name, std::string tubesheet_csv, std::string config_json) {
        try {
            std::filesystem::create_directory(hxs_path / hx_name);

            std::filesystem::path tubesheet_csv_path(hxs_path / hx_name / "tubesheet.csv");
            // Open a file for writing
            std::ofstream tubesheet_csv_file(tubesheet_csv_path);
            tubesheet_csv_file << tubesheet_csv;

            std::filesystem::path config_json_path(hxs_path / hx_name / "config.json");
            // Open a file for writing
            std::ofstream config_json_file(config_json_path);
            config_json_file << config_json;
            return true;
        } catch (const std::filesystem::filesystem_error &e) {
            std::cerr << "Error creating HX: " << e.what() << std::endl;
            return false;
        }
    }

    static bool erase(std::string hx_name) {
        try {
            std::filesystem::path normalized_path = std::filesystem::canonical(hxs_path / hx_name);
            std::filesystem::remove_all(normalized_path);
            return true;
        } catch (const std::filesystem::filesystem_error &e) {
            std::cerr << "Error deleting directory: " << e.what() << std::endl;
            return false;
        }
    }

    std::filesystem::path tubesheet_csv;
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

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(HX, tubesheet_csv,
        tubesheet_svg, tube_od,
        leg, unit, scale, tubes)

#endif // HX_HPP
