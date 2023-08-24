#ifndef POINTS_HPP
#define POINTS_HPP

class Point3D {
public:
    double x, y, z;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Point3D, x, y, z)

#endif 		// POINTS_HPP
