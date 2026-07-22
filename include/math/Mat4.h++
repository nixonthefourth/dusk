//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_MAT4_H
#define DUSK_MAT4_H

#include "math/Vec3.h++"

/** Minimal 4x4 matrix used for world-to-camera transforms. */
struct Mat4 {
    float m[4][4] =
    {
        {1.f, 0.f, 0.f, 0.f},
        {0.f, 1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f, 0.f},
        {0.f, 0.f, 0.f, 1.f}
    };
};

/** Transforms a 3D point by a matrix, assuming an implicit w value of 1. */
inline Vec3 transformPoint(const Mat4& matrix, const Vec3& point)
{
    return
    {
        matrix.m[0][0] * point.x + matrix.m[0][1] * point.y + matrix.m[0][2] * point.z + matrix.m[0][3],
        matrix.m[1][0] * point.x + matrix.m[1][1] * point.y + matrix.m[1][2] * point.z + matrix.m[1][3],
        matrix.m[2][0] * point.x + matrix.m[2][1] * point.y + matrix.m[2][2] * point.z + matrix.m[2][3]
    };
}

#endif //DUSK_MAT4_H
