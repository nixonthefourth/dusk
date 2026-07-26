//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_SHIP_PHYSICS_H
#define DUSK_SHIP_PHYSICS_H

#include "objects/ship.h++"
#include "math/verlet.h++"

/** Returns the current thrust force vector from ship controls. */
inline Vec3 shipThrustForce(const Ship& ship)
{
    const float thrustDirection = ship.reverseThrust ? -1.f : 1.f;
    return
        shipForward(ship) *
        ship.maxThrust *
        ship.throttle *
        thrustDirection;
}

/** Returns current ship acceleration, protecting the integrator from invalid mass. */
inline Vec3 shipAcceleration(const Ship& ship, const Vec3& externalAcceleration = {})
{
    if (ship.mass <= 0.f)
        return externalAcceleration;

    return shipThrustForce(ship) / ship.mass + externalAcceleration;
}

/** Integrates Newtonian ship motion from persistent velocity, thrust, and any external forces. */
inline void integrateShipPhysics(Ship& ship, float dt, const Vec3& externalAcceleration = {})
{
    ship.previousPosition = ship.position;

    const Vec3 acceleration = shipAcceleration(ship, externalAcceleration);

    ship.position = verlet::position_update(ship.position, ship.velocity, acceleration, dt);

    // Current thrust + gravity acceleration is treated as constant across this frame.
    ship.velocity = verlet::velocity_update(ship.velocity, acceleration, acceleration, dt);
}

/** Returns velocity magnitude in world units per second. */
inline float shipSpeed(const Ship& ship)
{
    return length(ship.velocity);
}

#endif //DUSK_SHIP_PHYSICS_H
