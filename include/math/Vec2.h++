//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_VEC2_H
#define DUSK_VEC2_H

#include <cmath>

/** Lightweight 2D vector for screen-space coordinates and simple math. */
struct Vec2
{
    float x = 0.f;
    float y = 0.f;
};

/* Compound operators */

/** Adds b into a. */
inline Vec2& operator+=(Vec2& a, const Vec2& b)
{
    a.x += b.x;
    a.y += b.y;
    return a;
}

/** Subtracts b from a. */
inline Vec2& operator-=(Vec2& a, const Vec2& b)
{
    a.x -= b.x;
    a.y -= b.y;
    return a;
}

/** Multiplies a by b component-by-component. */
inline Vec2& operator*=(Vec2& a, const Vec2& b)
{
    a.x *= b.x;
    a.y *= b.y;
    return a;
}

/** Multiplies a by a scalar. */
inline Vec2& operator*=(Vec2& a, float s)
{
    a.x *= s;
    a.y *= s;
    return a;
}

/** Divides a by b component-by-component. */
inline Vec2& operator/=(Vec2& a, const Vec2& b)
{
    a.x /= b.x;
    a.y /= b.y;
    return a;
}

/** Divides a by a scalar. */
inline Vec2& operator/=(Vec2& a, float s)
{
    a.x /= s;
    a.y /= s;
    return a;
}

/* Standard operators */

/** Returns the sum of two vectors. */
inline Vec2 operator+(Vec2 a, const Vec2& b)
{
    return a += b;
}

/** Returns the difference between two vectors. */
inline Vec2 operator-(Vec2 a, const Vec2& b)
{
    return a -= b;
}

/** Returns component-by-component multiplication. */
inline Vec2 operator*(Vec2 a, const Vec2& b)
{
    return a *= b;
}

/** Returns a vector multiplied by a scalar. */
inline Vec2 operator*(Vec2 a, float s)
{
    return a *= s;
}

/** Returns a vector multiplied by a scalar. */
inline Vec2 operator*(float s, Vec2 a)
{
    return a *= s;
}

/** Returns component-by-component division. */
inline Vec2 operator/(Vec2 a, const Vec2& b)
{
    return a /= b;
}

/** Returns a vector divided by a scalar. */
inline Vec2 operator/(Vec2 a, float s)
{
    return a /= s;
}

/* Utility functions */

/** Returns vector length. */
inline float length(const Vec2& v)
{
    return std::sqrt(v.x * v.x + v.y * v.y);
}

/** Returns a unit vector, or zero when the input is zero length. */
inline Vec2 normalized(const Vec2& v)
{
    float len = length(v);

    if (len == 0.f)
        return {0.f, 0.f};

    return
    {
        v.x / len,
        v.y / len
    };
}

/** Returns the dot product of two vectors. */
inline float dot(const Vec2& a, const Vec2& b)
{
    return
        a.x * b.x +
        a.y * b.y;
}

#endif // DUSK_VEC2_H
