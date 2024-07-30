#pragma once

#include <algorithm>
#include <cmath>
#include <iostream>
#include <regex>
#include <spdlog/spdlog.h>
#include <vector>

static inline float to_float(std::string input) {
    float res;
    if (input.empty()) {
        res = 0.0f; // Set to 0 if the input string is empty
    } else {
        try {
            res = std::stof(input);
        } catch (const std::invalid_argument& e) {
            // Handle invalid input (e.g., non-numeric string)
            SPDLOG_ERROR("Invalid input: {}", e.what());
            res = 0.0f; // Set to 0 in case of invalid input
        }
    }
    return res;
}

static inline double to_double(std::string input) {
    float res;
    if (input.empty()) {
        res = 0.0f; // Set to 0 if the input string is empty
    } else {
        try {
            res = std::stod(input);
        } catch (const std::invalid_argument& e) {
            // Handle invalid input (e.g., non-numeric string)
            SPDLOG_ERROR("Invalid input: {}", e.what());
            res = 0.0f; // Set to 0 in case of invalid input
        }
    }
    return res;
}

bool isValidIPv4(const std::string &str) {
    std::regex ipv4Regex("^\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}$");
    return std::regex_match(str, ipv4Regex);
}

