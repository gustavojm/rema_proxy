#ifndef CIRCLE_FIT_HPP
#define CIRCLE_FIT_HPP

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

struct Circle {
    Point3D center;
    double radius;
};

double distance(const Point3D& p1, const Point3D& p2) {
    return std::sqrt((p1.x - p2.x) * (p1.x - p2.x) + (p1.y - p2.y) * (p1.y - p2.y));
}

Circle fitCircle(const std::vector<Point3D>& points) {
    int n = points.size();
    if (n < 3) {
        throw std::invalid_argument("At least 3 points are required.");
    }

    // Shuffle the points to avoid bias from outliers
    std::vector<Point3D> shuffledPoints = points;
    std::random_shuffle(shuffledPoints.begin(), shuffledPoints.end());

    Point3D center = {0.0, 0.0, 0.0};
    double radius = 0.0;

    for (int i = 0; i < n; ++i) {
        center.x += shuffledPoints[i].x;
        center.y += shuffledPoints[i].y;
    }
    center.x /= n;
    center.y /= n;

    for (int i = 0; i < n; ++i) {
        double d = distance(center, shuffledPoints[i]);
        radius += d;
    }
    radius /= n;

    return {center, radius};
}

#endif     // CIRCLE_FIT_HPP
