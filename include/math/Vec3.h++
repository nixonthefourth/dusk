//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_VEC3_H
#define DUSK_VEC3_H

#include <cmath>

struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;
};

// Subtraction
inline Vec3 operator-(const Vec3& a, const Vec3& b)
{
    return
    {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z
    };
}

// Addition
inline Vec3 operator+(const Vec3& a, const Vec3& b)
{
    return
    {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z
    };
}

// Multiplication between objects
inline Vec3 operator*(const Vec3& a, const Vec3& b)
{
    return
    {
        a.x * b.x,
        a.y * b.y,
        a.z * b.z
    };
}

// Constant multiplication
inline Vec3 operator*(const Vec3& a, float b) {
    return
    {
      a.x * b,
        a.y * b,
        a.z * b
    };
}

// Division between objects
inline Vec3 operator/(const Vec3& a, const Vec3& b)
{
    return
    {
        a.x / b.x,
        a.y / b.y,
        a.z / b.z
    };
}

// Division by constant
inline Vec3 operator/(const Vec3& a, float b) {
    return
    {
        a.x / b,
        a.y / b,
        a.z / b
    };
}

// Magnitude (Length)
inline float length(const Vec3& v)
{
    return std::sqrt(
        v.x * v.x +
        v.y * v.y +
        v.z * v.z
    );
}

// Unit vector
inline Vec3 normalized(const Vec3& v)
{
    float len = length(v);

    if (len == 0.f)
        return {0.f, 0.f, 0.f};

    return
    {
        v.x / len,
        v.y / len,
        v.z / len
    };
}

// Dot product
inline float dot(const Vec3& a, const Vec3& b)
{
    return
        a.x * b.x +
        a.y * b.y +
        a.z * b.z;
}

// Cross product
inline Vec3 cross(const Vec3& a, const Vec3& b)
{
    return
    {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

/* Compound operators */

// Addition
inline Vec3 operator+=(const Vec3& a, const Vec3& b)
{
    return
    {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z
    };
}

// Subtraction
inline Vec3 operator-=(const Vec3& a, const Vec3& b)
{
    return
    {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z
    };
}

// Object Multiplication
inline Vec3 operator*=(const Vec3& a, const Vec3& b)
{
    return
    {
        a.x * b.x,
        a.y * b.y,
        a.z * b.z
    };
}

// Constant multiplication
inline Vec3 operator*=(const Vec3& a, float b) {
    return
    {
        a.x * b,
          a.y * b,
          a.z * b
      };
}

// Object Division
inline Vec3 operator/=(const Vec3& a, const Vec3& b)
{
    return
    {
        a.x / b.x,
        a.y / b.y,
        a.z / b.z
    };
}

// Constant Division
inline Vec3 operator/=(const Vec3& a, float b) {
    return
    {
        a.x / b,
        a.y / b,
        a.z / b
    };
}

#endif //DUSK_VEC3_H
