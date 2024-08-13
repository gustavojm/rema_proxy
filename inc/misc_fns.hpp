#pragma once

#include <algorithm>
#include <cmath>
#include <iostream>
#include <regex>
#include <spdlog/spdlog.h>
#include <vector>
#include <chrono>

inline float to_float(std::string input) {
    float res;
    if (input.empty()) {
        res = 0.0f; // Set to 0 if the input string is empty
    } else {
        try {
            res = std::stof(input);
        } catch (const std::invalid_argument &e) {
            // Handle invalid input (e.g., non-numeric string)
            SPDLOG_ERROR("Invalid input: {}", e.what());
            res = 0.0f; // Set to 0 in case of invalid input
        }
    }
    return res;
}

inline double to_double(std::string input) {
    float res;
    if (input.empty()) {
        res = 0.0f; // Set to 0 if the input string is empty
    } else {
        try {
            res = std::stod(input);
        } catch (const std::invalid_argument &e) {
            // Handle invalid input (e.g., non-numeric string)
            SPDLOG_ERROR("Invalid input: {}", e.what());
            res = 0.0f; // Set to 0 in case of invalid input
        }
    }
    return res;
}

inline bool isValidIPv4(const std::string &str) {
    std::regex ipv4Regex("^\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}$");
    return std::regex_match(str, ipv4Regex);
}

template<typename TP> std::time_t to_time_t(TP tp) {
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        tp - TP::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(sctp);
}

inline double timeDifference(const std::chrono::time_point<std::chrono::system_clock>& t1, const std::chrono::time_point<std::chrono::system_clock>& t2) {
    std::chrono::duration<double> diff = t2 - t1;
    return diff.count(); // Time difference in seconds
}