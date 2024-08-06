#pragma once

#include <chrono>

template<typename TP> std::time_t to_time_t(TP tp) {
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        tp - TP::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(sctp);
}

inline double timeDifference(const std::chrono::time_point<std::chrono::system_clock>& t1, const std::chrono::time_point<std::chrono::system_clock>& t2) {
    std::chrono::duration<double> diff = t2 - t1;
    return diff.count(); // Time difference in seconds
}