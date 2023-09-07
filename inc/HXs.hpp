#ifndef HXS_HPP
#define HXS_HPP

#include <vector>

#include "csv.h"

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

#endif      // HXS_HPP
