//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_VEC3_H
#define DUSK_VEC3_H

#include <cmath>

struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;
};

/* Compound operators */

// Addition
inline Vec3& operator+=(Vec3& a, const Vec3& b)
{
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}

// Subtraction
inline Vec3& operator-=(Vec3& a, const Vec3& b)
{
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
    return a;
}

// Component multiplication
inline Vec3& operator*=(Vec3& a, const Vec3& b)
{
    a.x *= b.x;
    a.y *= b.y;
    a.z *= b.z;
    return a;
}

// Scalar multiplication
inline Vec3& operator*=(Vec3& a, float s)
{
    a.x *= s;
    a.y *= s;
    a.z *= s;
    return a;
}

// Component division
inline Vec3& operator/=(Vec3& a, const Vec3& b)
{
    a.x /= b.x;
    a.y /= b.y;
    a.z /= b.z;
    return a;
}

// Scalar division
inline Vec3& operator/=(Vec3& a, float s)
{
    a.x /= s;
    a.y /= s;
    a.z /= s;
    return a;
}

/* Standard operators */

// Addition
inline Vec3 operator+(Vec3 a, const Vec3& b)
{
    return a += b;
}

// Subtraction
inline Vec3 operator-(Vec3 a, const Vec3& b)
{
    return a -= b;
}

// Component multiplication
inline Vec3 operator*(Vec3 a, const Vec3& b)
{
    return a *= b;
}

// Scalar multiplication
inline Vec3 operator*(Vec3 a, float s)
{
    return a *= s;
}

// Scalar multiplication (commutative)
inline Vec3 operator*(float s, Vec3 a)
{
    return a *= s;
}

// Component division
inline Vec3 operator/(Vec3 a, const Vec3& b)
{
    return a /= b;
}

// Scalar division
inline Vec3 operator/(Vec3 a, float s)
{
    return a /= s;
}

/* Utility functions */

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

#endif //DUSK_VEC3_H
