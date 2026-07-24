//
// Created by Mykyta Khomiakov on 24/07/2026.
//

#ifndef DUSK_PLANET_H
#define DUSK_PLANET_H

#include "math/Vec3.h++"
#include "objects/collision_body.h++"

/** A simple spherical world body rendered through the fake-3D projector. */
struct Planet {
    /** Center of the planet in world space. */
    Vec3 position;

    /** World-space sphere radius. */
    float radius = 1000.f;

    /** Object-level collision state, refreshed by World every physics update. */
    CollisionBody collision;

    /** Whether a projected ellipse ring should be drawn around the planet. */
    bool hasRing = false;

    /** Ring rotation in screen-space degrees. */
    float ringRotationDegrees = 0.f;

    /** Ring height as a fraction of its projected width. */
    float ringFlattening = 0.28f;
};

/** Returns the planet's current collision radius, derived from its visible sphere. */
inline float planetCollisionRadius(const Planet& planet)
{
    return planet.radius;
}

#endif //DUSK_PLANET_H
