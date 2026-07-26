//
// Created by Mykyta Khomiakov on 24/07/2026.
//

#ifndef DUSK_PLANET_H
#define DUSK_PLANET_H

#include "math/Vec3.h++"
#include "objects/collision_body.h++"

/** A simple spherical world body rendered through the fake-3D projector. */
struct Planet {
    Vec3 position;
    Vec3 velocity;
    float mass = 1.f;
    float radius = 1000.f;
    CollisionBody collision;
    bool hasRing = false;
    float ringRotationDegrees = 0.f;
    float ringFlattening = 0.28f;

    /** Marks this as the system's central star, rendered as a filled disc instead of a wire grid. */
    bool isStar = false;
};

/** Returns the planet's current collision radius, derived from its visible sphere. */
inline float planetCollisionRadius(const Planet& planet)
{
    return planet.radius;
}

#endif //DUSK_PLANET_H