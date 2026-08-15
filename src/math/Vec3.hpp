#pragma once

#include <cmath>

namespace webeast {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    constexpr Vec3 operator+(const Vec3& rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
    constexpr Vec3 operator-(const Vec3& rhs) const { return {x - rhs.x, y - rhs.y, z - rhs.z}; }
    constexpr Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    constexpr Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }

    Vec3& operator+=(const Vec3& rhs) { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
    Vec3& operator-=(const Vec3& rhs) { x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
};

inline constexpr float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float lengthSq(const Vec3& v) { return dot(v, v); }
inline float length(const Vec3& v) { return std::sqrt(lengthSq(v)); }

inline Vec3 normalized(const Vec3& v) {
    const float len = length(v);
    return len > 0.00001f ? v / len : Vec3{};
}

inline Vec3 clampMagnitude(const Vec3& v, float maxMagnitude) {
    const float lenSq = lengthSq(v);
    const float maxSq = maxMagnitude * maxMagnitude;
    if (lenSq <= maxSq || lenSq <= 0.000001f) return v;
    return v * (maxMagnitude / std::sqrt(lenSq));
}

} // namespace webeast
