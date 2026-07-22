//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_VEC2_H
#define DUSK_VEC2_H

#include <cmath>

struct Vec2
{
    float x = 0.f;
    float y = 0.f;
};

/* Compound operators */

// Addition
inline Vec2& operator+=(Vec2& a, const Vec2& b)
{
    a.x += b.x;
    a.y += b.y;
    return a;
}

// Subtraction
inline Vec2& operator-=(Vec2& a, const Vec2& b)
{
    a.x -= b.x;
    a.y -= b.y;
    return a;
}

// Component multiplication
inline Vec2& operator*=(Vec2& a, const Vec2& b)
{
    a.x *= b.x;
    a.y *= b.y;
    return a;
}

// Scalar multiplication
inline Vec2& operator*=(Vec2& a, float s)
{
    a.x *= s;
    a.y *= s;
    return a;
}

// Component division
inline Vec2& operator/=(Vec2& a, const Vec2& b)
{
    a.x /= b.x;
    a.y /= b.y;
    return a;
}

// Scalar division
inline Vec2& operator/=(Vec2& a, float s)
{
    a.x /= s;
    a.y /= s;
    return a;
}

/* Standard operators */

// Addition
inline Vec2 operator+(Vec2 a, const Vec2& b)
{
    return a += b;
}

// Subtraction
inline Vec2 operator-(Vec2 a, const Vec2& b)
{
    return a -= b;
}

// Component multiplication
inline Vec2 operator*(Vec2 a, const Vec2& b)
{
    return a *= b;
}

// Scalar multiplication
inline Vec2 operator*(Vec2 a, float s)
{
    return a *= s;
}

// Scalar multiplication (commutative)
inline Vec2 operator*(float s, Vec2 a)
{
    return a *= s;
}

// Component division
inline Vec2 operator/(Vec2 a, const Vec2& b)
{
    return a /= b;
}

// Scalar division
inline Vec2 operator/(Vec2 a, float s)
{
    return a /= s;
}

/* Utility functions */

// Magnitude
inline float length(const Vec2& v)
{
    return std::sqrt(v.x * v.x + v.y * v.y);
}

// Normalized vector
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

// Dot product
inline float dot(const Vec2& a, const Vec2& b)
{
    return
        a.x * b.x +
        a.y * b.y;
}

#endif // DUSK_VEC2_H
