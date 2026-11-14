// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
//
// Small vector type and constants. The world frame is right-handed with +y up;
// the horizontal plane is (x, z). Courses use x = east, z = south (so that
// y = up completes a right-handed frame); a shot's own frame is described in
// shot.hpp. Units are SI throughout: metres, seconds, kilograms, radians.
#pragma once

#include <cmath>

namespace golf {

inline constexpr double kPi = 3.14159265358979323846;
inline constexpr double kDeg = kPi / 180.0;
inline constexpr double kRpm = 2.0 * kPi / 60.0;  // rad/s per rpm
inline constexpr double kGravity = 9.80665;        // standard gravity (CGPM 1901), m/s^2
inline constexpr double kMph = 0.44704;            // m/s per mile per hour (exact)
inline constexpr double kYard = 0.9144;            // m per yard (exact)
inline constexpr double kFoot = 0.3048;            // m per foot (exact)

struct Vec3 {
    double x = 0, y = 0, z = 0;

    constexpr Vec3() = default;
    constexpr Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    constexpr Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator-() const { return {-x, -y, -z}; }
    constexpr Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    constexpr Vec3 operator/(double s) const { return {x / s, y / s, z / s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(double s) { x *= s; y *= s; z *= s; return *this; }
    constexpr bool operator==(const Vec3& o) const { return x == o.x && y == o.y && z == o.z; }

    constexpr double dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    constexpr Vec3 cross(const Vec3& o) const { return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x}; }
    double length() const { return std::sqrt(dot(*this)); }
    constexpr double length2() const { return dot(*this); }
    double horizontalLength() const { return std::sqrt(x * x + z * z); }
    Vec3 normalized() const {
        const double l = length();
        return l > 0 ? *this / l : Vec3{};
    }
};

inline constexpr Vec3 operator*(double s, const Vec3& v) { return v * s; }

inline constexpr Vec3 kUp{0, 1, 0};

inline double clamp(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline double lerp(double a, double b, double t) { return a + (b - a) * t; }
inline double smoothstep(double e0, double e1, double x) {
    const double t = clamp((x - e0) / (e1 - e0), 0.0, 1.0);
    return t * t * (3 - 2 * t);
}

// Component of v perpendicular to unit vector n.
inline Vec3 tangential(const Vec3& v, const Vec3& n) { return v - n * v.dot(n); }

// Rotate v about unit axis k by angle a (Rodrigues).
inline Vec3 rotate(const Vec3& v, const Vec3& k, double a) {
    const double c = std::cos(a), s = std::sin(a);
    return v * c + k.cross(v) * s + k * (k.dot(v) * (1 - c));
}

// Horizontal unit vector at heading h (radians) measured from +x toward +z.
inline Vec3 heading(double h) { return {std::cos(h), 0, std::sin(h)}; }
inline double headingOf(const Vec3& v) { return std::atan2(v.z, v.x); }

// Wrap an angle to (-pi, pi].
inline double wrapPi(double a) {
    while (a > kPi) a -= 2 * kPi;
    while (a <= -kPi) a += 2 * kPi;
    return a;
}

}  // namespace golf
