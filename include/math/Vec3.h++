//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_VEC3_H
#define DUSK_VEC3_H

#include <cmath>

/** Lightweight 3D vector for world-space and camera-space math. */
struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;
};

/* Compound operators */

/** Adds b into a. */
inline Vec3& operator+=(Vec3& a, const Vec3& b)
{
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}

/** Subtracts b from a. */
inline Vec3& operator-=(Vec3& a, const Vec3& b)
{
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
    return a;
}

/** Multiplies a by b component-by-component. */
inline Vec3& operator*=(Vec3& a, const Vec3& b)
{
    a.x *= b.x;
    a.y *= b.y;
    a.z *= b.z;
    return a;
}

/** Multiplies a by a scalar. */
inline Vec3& operator*=(Vec3& a, float s)
{
    a.x *= s;
    a.y *= s;
    a.z *= s;
    return a;
}

/** Divides a by b component-by-component. */
inline Vec3& operator/=(Vec3& a, const Vec3& b)
{
    a.x /= b.x;
    a.y /= b.y;
    a.z /= b.z;
    return a;
}

/** Divides a by a scalar. */
inline Vec3& operator/=(Vec3& a, float s)
{
    a.x /= s;
    a.y /= s;
    a.z /= s;
    return a;
}

/* Standard operators */

/** Returns the sum of two vectors. */
inline Vec3 operator+(Vec3 a, const Vec3& b)
{
    return a += b;
}

/** Returns the difference between two vectors. */
inline Vec3 operator-(Vec3 a, const Vec3& b)
{
    return a -= b;
}

/** Returns component-by-component multiplication. */
inline Vec3 operator*(Vec3 a, const Vec3& b)
{
    return a *= b;
}

/** Returns a vector multiplied by a scalar. */
inline Vec3 operator*(Vec3 a, float s)
{
    return a *= s;
}

/** Returns a vector multiplied by a scalar. */
inline Vec3 operator*(float s, Vec3 a)
{
    return a *= s;
}

/** Returns component-by-component division. */
inline Vec3 operator/(Vec3 a, const Vec3& b)
{
    return a /= b;
}

/** Returns a vector divided by a scalar. */
inline Vec3 operator/(Vec3 a, float s)
{
    return a /= s;
}

/* Utility functions */

/** Returns vector length. */
inline float length(const Vec3& v)
{
    return std::sqrt(
        v.x * v.x +
        v.y * v.y +
        v.z * v.z
    );
}

/** Returns a unit vector, or zero when the input is zero length. */
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

/** Returns the dot product of two vectors. */
inline float dot(const Vec3& a, const Vec3& b)
{
    return
        a.x * b.x +
        a.y * b.y +
        a.z * b.z;
}

/** Returns the cross product of two vectors. */
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
