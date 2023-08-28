#ifndef HXS_HPP
#define HXS_HPP

#include <vector>

#include "csv.h"

struct Tube {
    std::string x_label;
    std::string y_label;
    float cl_x;
    float cl_y;
    float hl_x;
    float hl_y;
    int tube_id;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Tube, x_label, y_label, cl_x, cl_y, hl_x,
        hl_y, tube_id)

static std::string hxs_path = "./HXs";

static inline std::vector<std::string> HXs_list() {
    std::vector<std::string> res;
    for (const auto &entry : std::filesystem::directory_iterator(hxs_path)) {
        if (entry.is_directory()) {
            res.push_back(entry.path().filename());
        }
    }
    return res;
}

static inline std::vector<Tube> HX_get_tubes(std::filesystem::path tubesheet_csv) {
    // Parse the CSV file to extract the data for each tube
    std::vector<Tube> tubes;
    try {
        io::CSVReader<7, io::trim_chars<' ', '\t'>, io::no_quote_escape<';'>> in(
                tubesheet_csv);
        in.read_header(io::ignore_extra_column, "x_label", "y_label", "cl_x",
                "cl_y", "hl_x", "hl_y", "tube_id");
        std::string x_label, y_label;
        float cl_x, cl_y, hl_x, hl_y;
        std::string tube_id;
        while (in.read_row(x_label, y_label, cl_x, cl_y, hl_x, hl_y, tube_id)) {
            tubes.push_back( { x_label, y_label, cl_x, cl_y, hl_x, hl_y,
                    std::stoi(tube_id.substr(5)) });
        }
        return tubes;
    } catch (std::exception &e) {
        std::cout << e.what();
        return tubes;
    }
}

#endif      // HXS_HPP
