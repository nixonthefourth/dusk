//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_SHIP_H
#define DUSK_SHIP_H

#include "math/Vec3.h++"
#include <algorithm>
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

/** Clamps ship pitch to keep the third-person camera readable. */
inline void clampShipPitch(Ship& ship)
{
    constexpr float maxPitch = 1.25f;
    ship.pitch = std::clamp(ship.pitch, -maxPitch, maxPitch);
}

#endif //DUSK_SHIP_H
