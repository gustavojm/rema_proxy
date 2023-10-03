#ifndef MISC_FNS_HPP
#define MISC_FNS_HPP

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

static inline float to_float(std::string input) {
    float res;
    if (input.empty()) {
            res = 0.0f; // Set to 0 if the input string is empty
        } else {
            try {
                res = std::stof(input);
            } catch (const std::invalid_argument& e) {
                // Handle invalid input (e.g., non-numeric string)
                std::cerr << "Invalid input: " << e.what() << std::endl;
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
                std::cerr << "Invalid input: " << e.what() << std::endl;
                res = 0.0f; // Set to 0 in case of invalid input
            }
        }
    return res;
}


#endif     // MISC_FNS_HPP
