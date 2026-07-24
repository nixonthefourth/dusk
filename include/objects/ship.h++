//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_SHIP_H
#define DUSK_SHIP_H

#include "io/obj_loader.h++"
#include "math/Vec3.h++"
#include "model/vector_model.h++"
#include <algorithm>
#include <cmath>
#include <string>

/** Creates the built-in Sidewinder-inspired vector model used when no OBJ is loaded. */
inline VectorModel createDefaultShipModel()
{
    return
    {
        {
            {0.f, 0.f, 430.f},
            {-260.f, -35.f, 100.f},
            {260.f, -35.f, 100.f},
            {-560.f, -70.f, -300.f},
            {560.f, -70.f, -300.f},
            {0.f, 120.f, -130.f},
            {0.f, -130.f, -130.f},
            {-260.f, 45.f, -470.f},
            {260.f, 45.f, -470.f},
            {-260.f, -85.f, -470.f},
            {260.f, -85.f, -470.f},
            {0.f, 85.f, 140.f}
        },
        {
            {0, 1, false}, {0, 2, false}, {1, 3, false}, {2, 4, false},
            {3, 7, false}, {4, 8, false}, {7, 8, false}, {5, 7, false},
            {5, 8, false}, {1, 5, false}, {2, 5, false}, {5, 11, false},
            {0, 11, false}, {11, 1, false}, {11, 2, false},
            {0, 6, true}, {1, 6, true}, {2, 6, true}, {3, 9, true},
            {4, 10, true}, {6, 9, true}, {6, 10, true}, {9, 10, true},
            {7, 9, true}, {8, 10, true}
        }
    };
}

/** Player ship state and tuning values. */
struct Ship {
    /** Ship origin in world space. */
    Vec3 position = {0.f, 0.f, 0.f};

    /** Ship velocity in world units per second. */
    Vec3 velocity;

    /** Horizontal heading in radians. */
    float yaw = 0.f;

    /** Nose pitch in radians. */
    float pitch = 0.f;

    /** Current throttle amount in the range [0, 1]. */
    float throttle = 0.f;

    /** When true, thrust is applied opposite the ship's forward axis. */
    bool reverseThrust = false;

    /** Acceleration at full throttle in world units per second squared. */
    float maxAcceleration = 80.f;

    /** W/S throttle change speed per second. */
    float throttleChangeSpeed = 0.5f;

    /** A/D turn speed in radians per second. */
    float yawSpeed = 0.8f;

    /** Q/E pitch speed in radians per second. */
    float pitchSpeed = 0.8f;

    /** Local-space vector model rendered for this ship. */
    VectorModel model = createDefaultShipModel();

    /** Replaces the ship's local vector model with an OBJ converted into vertices and edges. */
    bool loadObjModel(const std::string& path, const ObjLoadOptions& options = {})
    {
        const auto loadedModel = loadObjFileAsVectorModel(path, options);

        if (!loadedModel)
            return false;

        model = *loadedModel;
        return true;
    }
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

/** Keeps ship pitch within a readable nose-up/nose-down range. */
inline void clampShipPitch(Ship& ship)
{
    ship.yaw = wrapAngle(ship.yaw);

    constexpr float maxPitch = 1.25f;
    ship.pitch = std::clamp(ship.pitch, -maxPitch, maxPitch);
}

#endif //DUSK_SHIP_H
