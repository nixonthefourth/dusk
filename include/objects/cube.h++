//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_CUBE_H
#define DUSK_CUBE_H

#include "math/Vec3.h++"

/** Test object used to validate camera rotation and perspective projection. */
struct Cube {
    /** Center of the cube in world space. */
    Vec3 position = {0.f, 0.f, 4000.f};

    /** Euler rotation in radians. */
    Vec3 rotation;

    /** Edge length in world units. */
    float size = 600.f;

    /** Base angular speed in radians per second. */
    float rotationSpeed = 0.7f;
};

/** Advances the cube rotation with slightly different speeds per axis. */
inline void updateCube(Cube& cube, float dt)
{
    cube.rotation.x += cube.rotationSpeed * 0.73f * dt;
    cube.rotation.y += cube.rotationSpeed * dt;
    cube.rotation.z += cube.rotationSpeed * 0.41f * dt;
}

#endif //DUSK_CUBE_H
