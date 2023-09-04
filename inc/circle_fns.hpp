#ifndef CIRCLE_FNS_HPP
#define CIRCLE_FNS_HPP

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

#include "points.hpp"

struct Circle {
    Point3D center;
    double radius;
};

static inline double distance(const Point3D& p1, const Point3D& p2) {
    return std::sqrt((p1.x - p2.x) * (p1.x - p2.x) + (p1.y - p2.y) * (p1.y - p2.y));
}

static inline Circle fitCircle(const std::vector<Point3D>& points) {
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


static inline std::vector<Point3D> calculateCirclePoints(Point3D center, double radius, int numPoints) {
    std::vector<Point3D> points;

    double angleIncrement = 360.0 / numPoints;

    for (int i = 0; i < numPoints; ++i) {
        double angle = i * angleIncrement;
        double radians = angle * (M_PI / 180.0); // Convert degrees to radians

        Point3D point;
        point.x = center.x + radius * cos(radians);
        point.y = center.y + radius * sin(radians);

        points.push_back({point.x, point.y, 0});
        //std::cout << "Point " << i + 1 << ": (" << point.x << ", " << point.y << ")\n";
    }
    return points;
}

#endif     // CIRCLE_FNS_HPP
