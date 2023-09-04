#ifndef POINTS_HPP
#define POINTS_HPP

class Point3D {
public:

    Point3D() {};

    Point3D(double x, double y, double z) : x(x), y(y), z(z) {};

    Point3D operator*(double scalar) const {
        return Point3D(x * scalar, y * scalar, z * scalar);
    }


    Point3D operator/(double scalar) const {
    if (scalar != 0) {
           return Point3D(x / scalar, y / scalar, z / scalar);
       } else {
           std::cerr << "Error: Division by zero" << std::endl;
           // You can handle the error however you prefer
           return *this; // Return the original point in this case
       }
    }

    double x = 0;
    double y = 0;
    double z = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Point3D, x, y, z)

#endif 		// POINTS_HPP
