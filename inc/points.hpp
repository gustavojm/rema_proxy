#ifndef POINTS_HPP
#define POINTS_HPP

class Point3D {
public:

    Point3D() {};

    Point3D(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {};

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

    // Overloaded addition operator for Point3D + Point3D
    Point3D operator+(const Point3D& other) const {
        return Point3D(x + other.x, y + other.y, z + other.z);
    }

    // Overloaded subtraction operator for Point3D - Point3D
    Point3D operator-(const Point3D& other) const {
        return Point3D(x - other.x, y - other.y, z - other.z);
    }

    // Overloaded += operator for Point3D += Point3D
    Point3D& operator+=(const Point3D& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    // Overloaded -= operator for Point3D -= Point3D
    Point3D& operator-=(const Point3D& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    // Overloaded *= operator for Point3D *= scalar
    Point3D& operator*=(double scalar) {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    // Overloaded /= operator for Point3D /= scalar
    Point3D& operator/=(double scalar) {
        if (scalar != 0) {
            x /= scalar;
            y /= scalar;
            z /= scalar;
            return *this;
        } else {
            throw std::runtime_error("Division by zero");
        }
    }

    // Overloaded output operator for easy printing
    friend std::ostream& operator<<(std::ostream& os, const Point3D& point) {
        os << "(" << point.x << ", " << point.y << ", " << point.z << ")";
        return os;
    }

    double x = 0;
    double y = 0;
    double z = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Point3D, x, y, z)

#endif 		// POINTS_HPP
