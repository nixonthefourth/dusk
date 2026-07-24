//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_CUBE_H
#define DUSK_CUBE_H

#include "math/Vec3.h++"
#include "objects/collision_body.h++"

/** Test object used to validate camera rotation and perspective projection. */
struct Cube {
    /** Center of the cube in world space. */
    Vec3 position = {0.f, 0.f, 4000.f};

    /** Euler rotation in radians. */
    Vec3 rotation;

    /** Edge length in world units. */
    float size = 600.f;

    /** Object-level collision state, refreshed by World every physics update. */
    CollisionBody collision;

    /** Sphere scale used for the cube's portal-like collision volume. */
    float collisionRadiusScale = 0.56f;

    /** Base angular speed in radians per second. */
    float rotationSpeed = 0.7f;
};

/** Returns the cube's current collision radius, derived from its object size. */
inline float cubeCollisionRadius(const Cube& cube)
{
    return cube.size * cube.collisionRadiusScale;
}

/** Advances the cube rotation with slightly different speeds per axis. */
inline void updateCube(Cube& cube, float dt)
{
    cube.rotation.x += cube.rotationSpeed * 0.73f * dt;
    cube.rotation.y += cube.rotationSpeed * dt;
    cube.rotation.z += cube.rotationSpeed * 0.41f * dt;
}

#endif //DUSK_CUBE_H
