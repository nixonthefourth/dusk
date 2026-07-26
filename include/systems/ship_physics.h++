//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_SHIP_PHYSICS_H
#define DUSK_SHIP_PHYSICS_H

#include "objects/ship.h++"
#include "math/verlet.h++"

/** Integrates Newtonian ship motion from persistent velocity and current thrust. */
inline void integrateShipPhysics(Ship& ship, float dt)
{
    ship.previousPosition = ship.position;

    const float thrustDirection = ship.reverseThrust ? -1.f : 1.f;
    const Vec3 force =
        shipForward(ship) *
        ship.maxThrust *
        ship.throttle *
        thrustDirection;

    const Vec3 acceleration = force / ship.mass;

    ship.position = verlet::position_update(ship.position, ship.velocity, acceleration, dt);

    const Vec3 acceleration_new = force / ship.mass;

    ship.velocity = verlet::velocity_update(ship.velocity, acceleration, acceleration_new, dt);
}

/** Returns velocity magnitude in world units per second. */
inline float shipSpeed(const Ship& ship)
{
    return length(ship.velocity);
}

#endif //DUSK_SHIP_PHYSICS_H
