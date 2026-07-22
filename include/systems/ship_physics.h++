//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_SHIP_PHYSICS_H
#define DUSK_SHIP_PHYSICS_H

#include "objects/ship.h++"

/** Integrates Newtonian ship motion from persistent velocity and current thrust. */
inline void integrateShipPhysics(Ship& ship, float dt)
{
    const float thrustDirection = ship.reverseThrust ? -1.f : 1.f;
    const Vec3 acceleration =
        shipForward(ship) *
        ship.maxAcceleration *
        ship.throttle *
        thrustDirection;

    ship.velocity += acceleration * dt;
    ship.position += ship.velocity * dt;
}

/** Returns velocity magnitude in world units per second. */
inline float shipSpeed(const Ship& ship)
{
    return length(ship.velocity);
}

#endif //DUSK_SHIP_PHYSICS_H
