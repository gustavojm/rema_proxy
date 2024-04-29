#ifndef HXS_HPP
#define HXS_HPP

#include <filesystem>
#include <vector>

#include "csv.h"

static std::filesystem::path hxs_path = "./HXs";

static inline std::vector<std::string> HXs_list() {
    std::vector<std::string> res;
    for (const auto &entry : std::filesystem::directory_iterator(hxs_path)) {
        if (entry.is_directory()) {
            res.push_back(entry.path().filename());
        }
    }
    return res;
}

static inline bool HXs_create(std::string hx_name, std::string tubesheet_csv, std::string config_json) {
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

static inline bool HXs_delete(std::string hx_name) {
    try {
        std::filesystem::path normalized_path = std::filesystem::canonical(hxs_path / hx_name);
        std::filesystem::remove_all(normalized_path);
        return true;
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Error deleting directory: " << e.what() << std::endl;
        return false;
    }
}

#endif // HXS_HPP
