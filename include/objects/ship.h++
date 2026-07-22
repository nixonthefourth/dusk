//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_SHIP_H
#define DUSK_SHIP_H

#include "math/Vec3.h++"
#include <cmath>

/** Player ship state and tuning values. */
struct Ship {
    /** Ship origin in world space. */
    Vec3 position = {0.f, 0.f, 0.f};

    /** Horizontal heading in radians. */
    float yaw = 0.f;

    /** Nose pitch in radians. */
    float pitch = 0.f;

    /** Movement speed along the ship's local forward axis. */
    float speed = 400.f;

    /** A/D turn speed in radians per second. */
    float yawSpeed = 1.f;

    /** Q/E pitch speed in radians per second. */
    float pitchSpeed = 0.8f;
};

/** Returns the ship's local forward direction. */
inline Vec3 shipForward(const Ship& ship)
{
    const float cosPitch = std::cos(ship.pitch);

    return normalized(
    {
        std::sin(ship.yaw) * cosPitch,
        std::sin(ship.pitch),
        std::cos(ship.yaw) * cosPitch
    });
}

/** Returns the ship's local right direction. */
inline Vec3 shipRight(const Ship& ship)
{
    return normalized(
    {
        std::cos(ship.yaw),
        0.f,
        -std::sin(ship.yaw)
    });
}

/** Returns the ship's local up direction. */
inline Vec3 shipUp(const Ship& ship)
{
    return normalized(cross(shipForward(ship), shipRight(ship)));
}

/** Converts a local model-space point into world space using the ship orientation. */
inline Vec3 shipLocalToWorld(const Ship& ship, const Vec3& local)
{
    return
        ship.position +
        shipRight(ship) * local.x +
        shipUp(ship) * local.y +
        shipForward(ship) * local.z;
}

/** Wraps an angle so continuous pitch loops do not grow without bound. */
inline float wrapAngle(float angle)
{
    constexpr float pi = 3.14159265358979323846f;
    constexpr float tau = pi * 2.f;

    while (angle > pi)
        angle -= tau;

    while (angle < -pi)
        angle += tau;

    return angle;
}

/** Keeps yaw and pitch in a stable circular range while still allowing full loops. */
inline void wrapShipAngles(Ship& ship)
{
    ship.yaw = wrapAngle(ship.yaw);
    ship.pitch = wrapAngle(ship.pitch);
}

#endif //DUSK_SHIP_H
