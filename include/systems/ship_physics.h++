//
// Created by Mykyta Khomiakov on 22/07/2026.
//
// The ship flight model: flight assist, manual (Newtonian) thrust, rate-controlled rotation, the
// cruise drive with its charge-up and mass locking, all fed through velocity Verlet.
//

#ifndef DUSK_SHIP_PHYSICS_H
#define DUSK_SHIP_PHYSICS_H

#include "objects/ship.h++"
#include "math/verlet.h++"
#include <algorithm>
#include <cmath>

/*
 * Ship flight model.
 *
 * Everything that moves the ship is still a force divided by mass and fed through velocity
 * Verlet, so gravity, thrust and (later) cargo mass all compose naturally. What changed is who
 * decides the forces:
 *
 *  - Flight assist ON (default): throttle is a speed demand along the nose. Every step the
 *    flight computer works out the velocity error, splits the correcting acceleration into the
 *    ship's forward / right / up axes, and clamps each against what the main engine, the retro
 *    thrusters and the RCS can actually deliver. Sideways drift is bled off by the RCS, so the
 *    velocity follows the nose through a turn instead of sliding along the old heading.
 *
 *  - Flight assist OFF: the original model. Throttle is a direct fraction of main-engine thrust
 *    and the ship keeps whatever velocity it builds.
 *
 *  - Cruise: an in-system drive for crossing the (now much larger) systems. It is deliberately
 *    not Newtonian: speed is set along the nose, capped by distance to the nearest mass, and the
 *    drive drops out inside a body's mass-lock zone.
 *
 * Rotation is rate-controlled in every mode: input commands a yaw/pitch rate, the RCS spins the
 * ship up toward it, and stops it crisply on release, so short taps give small, exact corrections.
 */

/* ---- Mass and fuel ---------------------------------------------------------------------------- */

/** Everything the thrusters have to move: hull, fuel and cargo, in tonnes. */
inline float shipTotalMass(const Ship& ship)
{
    return ship.mass + std::max(0.f, ship.fuel) + std::max(0.f, ship.cargoMass);
}

/** The mass the handling is tuned at: the hull with half a tank. */
inline float shipReferenceMass(const Ship& ship)
{
    return ship.mass + ship.fuelCapacity * 0.5f;
}

/**
 * Total mass relative to the tuning mass (1 at half a tank, below 1 when lighter, above when
 * heavier). Rotation uses it: with more inertia for the RCS to fight, turns spin up and stop more
 * slowly and top turn rate falls; a light ship is correspondingly snappier.
 */
inline float shipMassRatio(const Ship& ship)
{
    const float reference = shipReferenceMass(ship);
    return reference > 0.f ? shipTotalMass(ship) / reference : 1.f;
}

/** How far a hyperspace jump can reach on the fuel aboard, in light years. */
inline float jumpRangeLightYears(const Ship& ship)
{
    if (!ship.usesFuel)
        return 1e9f;

    return ship.hyperspaceFuelPerLightYear > 0.f ? ship.fuel / ship.hyperspaceFuelPerLightYear : 1e9f;
}

/** Fuel a jump of `lightYears` would burn. */
inline float jumpFuelCost(const Ship& ship, float lightYears)
{
    return ship.usesFuel ? lightYears * ship.hyperspaceFuelPerLightYear : 0.f;
}

/** True when there is enough fuel to start or keep the cruise drive running. */
inline bool hasCruiseFuel(const Ship& ship)
{
    return !ship.usesFuel || ship.fuel > 0.001f;
}

/** Signed speed demand along the nose, in world units per second, from throttle and direction. */
inline float shipTargetSpeed(const Ship& ship)
{
    const float throttle = std::clamp(ship.throttle, 0.f, 1.f);

    if (ship.reverseThrust)
        return -throttle * ship.maxSpeed * ship.reverseSpeedFraction;

    return throttle * ship.maxSpeed;
}

/** Thrust force from flight assist: whatever the thrusters can give toward holding the demanded velocity. */
inline Vec3 flightAssistThrustForce(const Ship& ship, float dt, const Vec3& externalAcceleration)
{
    const float mass = shipTotalMass(ship);

    if (mass <= 0.f)
        return {};

    const Vec3 forward = shipForward(ship);
    const Vec3 right = shipRight(ship);
    const Vec3 up = shipUp(ship);

    const Vec3 desiredVelocity = forward * shipTargetSpeed(ship);

    // Close the error over one response time, but never by more than one step's worth, so a long
    // frame can't overshoot. External acceleration (gravity) is fed forward and cancelled.
    const float responseTime = std::max(ship.assistResponseTime, dt);
    const Vec3 wanted = (desiredVelocity - ship.velocity) / responseTime - externalAcceleration;

    float forwardAcceleration = dot(wanted, forward);
    float rightAcceleration = dot(wanted, right);
    float upAcceleration = dot(wanted, up);

    forwardAcceleration = std::clamp(
        forwardAcceleration,
        -ship.retroThrust / mass,
        ship.maxThrust / mass
    );

    // The RCS budget is shared across the lateral plane, so diagonal drift isn't corrected
    // faster than straight sideways drift.
    const float lateralLimit = ship.lateralThrust / mass;
    const float lateralAcceleration = std::hypot(rightAcceleration, upAcceleration);

    if (lateralAcceleration > lateralLimit)
    {
        const float scale = lateralLimit / lateralAcceleration;
        rightAcceleration *= scale;
        upAcceleration *= scale;
    }

    return (forward * forwardAcceleration + right * rightAcceleration + up * upAcceleration) * mass;
}

/** Thrust force with flight assist off: throttle drives the main engine (or retros, in reverse) directly. */
inline Vec3 manualThrustForce(const Ship& ship)
{
    const float throttle = std::clamp(ship.throttle, 0.f, 1.f);
    const float force = ship.reverseThrust ? -ship.retroThrust : ship.maxThrust;
    return shipForward(ship) * force * throttle;
}

/** Current thrust force vector, from whichever control mode is active. */
inline Vec3 shipThrustForce(const Ship& ship, float dt = 0.f, const Vec3& externalAcceleration = {})
{
    return ship.flightAssist
        ? flightAssistThrustForce(ship, dt, externalAcceleration)
        : manualThrustForce(ship);
}

/** Returns current ship acceleration, protecting the integrator from invalid mass. */
inline Vec3 shipAcceleration(const Ship& ship, float dt = 0.f, const Vec3& externalAcceleration = {})
{
    const float mass = shipTotalMass(ship);

    if (mass <= 0.f)
        return externalAcceleration;

    return shipThrustForce(ship, dt, externalAcceleration) / mass + externalAcceleration;
}

/** Eases a turn rate toward its commanded value: gentle spin-up, quick stop. */
inline float approachTurnRate(const Ship& ship, float rate, float commanded, float dt)
{
    const bool slowingDown = std::abs(commanded) < std::abs(rate) || commanded * rate < 0.f;

    // A heavier ship has more inertia for the RCS to fight: spin-up and stopping both take longer.
    const float inertia = shipMassRatio(ship);
    const float timeConstant = std::max(1e-3f, (slowingDown ? ship.turnStopTime : ship.turnResponseTime) * inertia);
    return commanded + (rate - commanded) * std::exp(-dt / timeConstant);
}

/** Integrates yaw and pitch from the pilot's rate demands. Autopilots that set yaw/pitch directly leave the inputs at zero. */
inline void integrateShipRotation(Ship& ship, float dt)
{
    // Top turn rate scales with the inverse square root of the mass ratio: about 8% slower with a
    // full tank than at half, about 10% quicker nearly empty.
    const float scale = (ship.precisionInput ? ship.precisionTurnScale : 1.f) / std::sqrt(shipMassRatio(ship));
    const float yawInput = std::clamp(ship.yawInput, -1.f, 1.f);
    const float pitchInput = std::clamp(ship.pitchInput, -1.f, 1.f);

    ship.yawRate = approachTurnRate(ship, ship.yawRate, yawInput * ship.yawSpeed * scale, dt);
    ship.pitchRate = approachTurnRate(ship, ship.pitchRate, pitchInput * ship.pitchSpeed * scale, dt);

    ship.yaw += ship.yawRate * dt;
    ship.pitch += ship.pitchRate * dt;

    const float pitchBefore = ship.pitch;
    clampShipPitch(ship);

    // Hitting the pitch stop kills the pitch rate rather than letting it wind up behind the clamp.
    if (ship.pitch != pitchBefore)
        ship.pitchRate = 0.f;
}

/** Clears all rotation demand and angular velocity; used when an autopilot hands control back. */
inline void resetShipRotationState(Ship& ship)
{
    ship.yawRate = 0.f;
    ship.pitchRate = 0.f;
    ship.yawInput = 0.f;
    ship.pitchInput = 0.f;
    ship.precisionInput = false;
}

/** True when the ship is too close to a planet, star or station to use the cruise drive. */
inline bool shipMassLocked(const Ship& ship)
{
    return ship.cruiseMargin <= 0.f;
}

/** Highest speed cruise allows right now: full cruise speed in deep space, tapering to maxSpeed at the mass-lock boundary. */
inline float cruiseSpeedLimit(const Ship& ship)
{
    return std::min(
        ship.cruiseMaxSpeed,
        ship.maxSpeed + ship.cruiseSlowdownRate * std::max(0.f, ship.cruiseMargin)
    );
}

/** Points throttle at the ship's current forward speed, so flight assist carries on rather than braking. */
inline void matchThrottleToVelocity(Ship& ship)
{
    const float forwardSpeed = dot(ship.velocity, shipForward(ship));

    if (forwardSpeed >= 0.f)
    {
        ship.reverseThrust = false;
        ship.throttle = ship.maxSpeed > 0.f ? std::clamp(forwardSpeed / ship.maxSpeed, 0.f, 1.f) : 0.f;
        return;
    }

    const float reverseSpeed = ship.maxSpeed * ship.reverseSpeedFraction;
    ship.reverseThrust = true;
    ship.throttle = reverseSpeed > 0.f ? std::clamp(-forwardSpeed / reverseSpeed, 0.f, 1.f) : 0.f;
}

/** Engages cruise if the ship is clear of every mass-lock zone. Returns whether cruise is now engaged. */
inline bool engageCruise(Ship& ship)
{
    if (ship.cruiseEngaged)
        return true;

    if (shipMassLocked(ship) || !hasCruiseFuel(ship))
        return false;

    // Cruise speed starts from the current forward speed; throttle now scales cruiseMaxSpeed.
    ship.cruiseEngaged = true;
    ship.reverseThrust = false;
    return true;
}

/** True while the cruise drive is spooling up. */
inline bool cruiseCharging(const Ship& ship)
{
    return ship.cruiseCharge >= 0.f;
}

/** Starts charging the cruise drive (refused while mass-locked or already cruising). Returns whether it started. */
inline bool beginCruiseCharge(Ship& ship)
{
    if (ship.cruiseEngaged || cruiseCharging(ship) || shipMassLocked(ship) || !hasCruiseFuel(ship))
        return false;

    ship.cruiseCharge = 0.f;
    return true;
}

/** Abandons a charge in progress. */
inline void cancelCruiseCharge(Ship& ship)
{
    ship.cruiseCharge = -1.f;
}

/** Advances a charge; a mass lock aborts it, and a full charge engages cruise. */
inline void updateCruiseCharge(Ship& ship, float dt)
{
    if (!cruiseCharging(ship))
        return;

    if (shipMassLocked(ship) || !hasCruiseFuel(ship))
    {
        cancelCruiseCharge(ship);
        return;
    }

    ship.cruiseCharge += dt / std::max(0.01f, ship.cruiseChargeTime);

    if (ship.cruiseCharge >= 1.f)
    {
        cancelCruiseCharge(ship);
        engageCruise(ship);
    }
}

/** Drops out of cruise: speed collapses to normal-space limits and throttle is re-pointed at it. */
inline void disengageCruise(Ship& ship)
{
    if (!ship.cruiseEngaged)
        return;

    ship.cruiseEngaged = false;

    const float speed = length(ship.velocity);

    if (speed > ship.maxSpeed)
        ship.velocity = ship.velocity * (ship.maxSpeed / speed);

    // With flight assist, throttle keeps the ship coasting at its exit speed. Without it,
    // throttle is raw thrust, so it is cut instead of firing the engine unasked.
    if (ship.flightAssist)
        matchThrottleToVelocity(ship);
    else
        ship.throttle = 0.f;
}

/** Kinematic cruise step: speed follows throttle along the nose, capped by distance to the nearest mass. */
inline void integrateCruise(Ship& ship, float dt)
{
    const Vec3 forward = shipForward(ship);
    const float limit = cruiseSpeedLimit(ship);
    const float target = std::min(std::clamp(ship.throttle, 0.f, 1.f) * ship.cruiseMaxSpeed, limit);

    float speed = std::max(0.f, dot(ship.velocity, forward));

    if (speed < target)
        speed = std::min(target, speed + ship.cruiseAcceleration * dt);
    else
        speed = std::max(target, speed - ship.cruiseDeceleration * dt);

    // The limit is hard: approaching a planet bleeds speed off continuously, so the drop-out at
    // the mass-lock boundary happens close to normal-space speed.
    speed = std::min(speed, limit);

    ship.velocity = forward * speed;
    ship.position += ship.velocity * dt;

    // The drive burns fuel in proportion to speed; running dry drops the ship out of cruise.
    if (ship.usesFuel && ship.cruiseMaxSpeed > 0.f)
    {
        ship.fuel = std::max(0.f, ship.fuel - ship.cruiseFuelPerSecond * (speed / ship.cruiseMaxSpeed) * dt);

        if (!hasCruiseFuel(ship))
            disengageCruise(ship);
    }
}

/**
 * Integrates one physics step: rotation from the pilot's rate demands, then either cruise or
 * Newtonian motion from thrust plus external forces (gravity), through velocity Verlet.
 * World must refresh ship.cruiseMargin before calling this.
 */
inline void integrateShipPhysics(Ship& ship, float dt, const Vec3& externalAcceleration = {})
{
    ship.previousPosition = ship.position;

    integrateShipRotation(ship, dt);
    updateCruiseCharge(ship, dt);

    if (ship.cruiseEngaged && shipMassLocked(ship))
        disengageCruise(ship);

    if (ship.cruiseEngaged)
    {
        integrateCruise(ship, dt);
        return;
    }

    const Vec3 acceleration = shipAcceleration(ship, dt, externalAcceleration);

    ship.position = verlet::position_update(ship.position, ship.velocity, acceleration, dt);

    // Thrust + gravity acceleration is treated as constant across this (short) step.
    ship.velocity = verlet::velocity_update(ship.velocity, acceleration, acceleration, dt);
}

/** Returns velocity magnitude in world units per second. */
inline float shipSpeed(const Ship& ship)
{
    return length(ship.velocity);
}

#endif //DUSK_SHIP_PHYSICS_H
