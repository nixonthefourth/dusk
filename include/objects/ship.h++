//
// Created by Mykyta Khomiakov on 22/07/2026.
//
// The Ship: position, velocity and orientation, the flight-model tuning (thrust, speed, turn
// rates, cruise), pilot intent written by the input layer, and the wireframe model. Also the
// orientation helpers (forward/right/up vectors, local-to-world transform) used everywhere.
//

#ifndef DUSK_SHIP_H
#define DUSK_SHIP_H

#include "io/obj_loader.h++"
#include "math/Vec3.h++"
#include "model/vector_model.h++"
#include "objects/collision_body.h++"
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
        },
        {
            {0, 11, 1}, {0, 2, 11},
            {1, 11, 5}, {2, 5, 11},
            {1, 7, 3}, {1, 5, 7},
            {2, 4, 8}, {2, 8, 5},
            {5, 8, 7},
            {0, 1, 6}, {0, 6, 2},
            {1, 3, 9}, {1, 9, 6},
            {2, 10, 4}, {2, 6, 10},
            {6, 9, 10},
            {3, 7, 9}, {4, 10, 8},
            {7, 8, 10}, {7, 10, 9}
        }
    };
}

/** Player ship state and tuning values. */
struct Ship {
    /** Ship origin in world space. */
    Vec3 position = {0.f, 0.f, 0.f};

    /** Ship origin before the latest physics integration step. */
    Vec3 previousPosition = {0.f, 0.f, 0.f};

    /** Ship velocity in world units per second. */
    Vec3 velocity;

    /** Object-level collision state, refreshed by World every physics update. */
    CollisionBody collision;

    /** Coarse spherical collision radius around the ship origin. */
    float collisionRadius = 140.f;

    /** Horizontal heading in radians. */
    float yaw = 0.f;

    /** Nose pitch in radians. */
    float pitch = 0.f;

    /** Roll around the forward axis in radians. Player controls leave this at zero; the docking computer uses it. */
    float roll = 0.f;

    /**
     * Current throttle amount in the range [0, 1]. With flight assist on this is a speed demand
     * (a fraction of maxSpeed, or of cruiseMaxSpeed while cruising); with it off, it is a direct
     * fraction of main-engine thrust, exactly as in the original Newtonian model.
     */
    float throttle = 0.f;

    /** When true, the ship flies (or thrusts) backwards along its forward axis. */
    bool reverseThrust = false;

    /* ---- Linear flight model --------------------------------------------------------------- */

    /*
     * Thruster forces. Acceleration is force / total mass, and the forces are tuned so the ship
     * handles exactly as it always has with half a tank (18 t): 70 u/s^2 forward, 90 braking,
     * 200 sideways. A full tank (21 t) is about 15% more sluggish; a nearly empty one (15 t)
     * about 20% livelier.
     */

    /** Main engine force at full throttle: 70 u/s^2 at half a tank, about 18 seconds from rest to top speed. */
    float maxThrust = 1260.f;

    /** Retro thrusters, used for braking and for flying in reverse (90 u/s^2 at half a tank). */
    float retroThrust = 1620.f;

    /**
     * RCS force along the ship's right and up axes (200 u/s^2 at half a tank). Flight assist spends it
     * cancelling sideways drift, which is what makes the velocity follow the nose through a turn.
     * Deliberately stronger than the main engine: turning is crisp, straight-line speed builds slowly.
     */
    float lateralThrust = 3600.f;

    /**
     * Dry hull mass in tonnes: the ship with empty tanks and holds. Thrusters divide their force
     * by the ship's *total* mass (shipTotalMass(): hull + fuel + cargo), so a full tank makes the
     * ship noticeably slower to accelerate and to turn.
     */
    float mass = 15.f;

    /* ---- Fuel ------------------------------------------------------------------------------- */

    /** Tank size and current fuel, in tonnes. Fuel counts toward the ship's mass. */
    float fuelCapacity = 6.f;
    float fuel = 6.f;

    /** Hyperspace cost: a full 6 t tank reaches 40 light years. */
    float hyperspaceFuelPerLightYear = 0.15f;

    /** Cruise-drive burn at full cruise speed, tonnes per second (scales with speed): ten minutes of flat-out cruising per tank. */
    float cruiseFuelPerSecond = 0.01f;

    /** False for ships that never run dry (NPCs): they burn nothing and never refuel. */
    bool usesFuel = true;

    /** Mass of carried cargo in tonnes. Nothing loads cargo yet; it is here so trading can add mass later. */
    float cargoMass = 0.f;

    /** When true, the flight computer converts throttle into a velocity and fires thrusters to hold it. */
    bool flightAssist = true;

    /** Speed flight assist holds at full throttle in normal space, in world units per second. */
    float maxSpeed = 1200.f;

    /** Fraction of maxSpeed available while flying in reverse. */
    float reverseSpeedFraction = 0.35f;

    /** Seconds flight assist takes to close a small velocity error; larger errors are thrust-limited. */
    float assistResponseTime = 0.3f;

    /** W/S throttle change speed per second. */
    float throttleChangeSpeed = 0.5f;

    /* ---- Rotation ---------------------------------------------------------------------------- */

    /** Maximum yaw rate in radians per second. */
    float yawSpeed = 1.35f;

    /** Maximum pitch rate in radians per second. */
    float pitchSpeed = 1.35f;

    /** Current yaw rate in radians per second (RCS-driven angular velocity). */
    float yawRate = 0.f;

    /** Current pitch rate in radians per second. */
    float pitchRate = 0.f;

    /** Seconds the yaw/pitch rate takes to spin up toward the commanded rate. */
    float turnResponseTime = 0.12f;

    /** Seconds the rate takes to die away once input is released or reversed, so the nose stops where it is aimed. */
    float turnStopTime = 0.05f;

    /** Turn-rate multiplier while the precision modifier is held. */
    float precisionTurnScale = 0.35f;

    /** Pilot intent written by the input layer each frame: yaw and pitch demands in [-1, 1]. */
    float yawInput = 0.f;
    float pitchInput = 0.f;

    /** True while the pilot holds the precision modifier (reduced turn rates for fine aiming). */
    bool precisionInput = false;

    /* ---- In-system cruise ------------------------------------------------------------------ */

    /** True while the in-system cruise drive is engaged. */
    bool cruiseEngaged = false;

    /**
     * Cruise spool-up progress in [0, 1) while the drive is charging, or a negative value when it
     * isn't. The pilot's J starts a charge; NPCs engage instantly through engageCruise().
     */
    float cruiseCharge = -1.f;

    /** Seconds the drive takes to charge before cruise engages. */
    float cruiseChargeTime = 1.2f;

    /** Cruise speed at full throttle, in world units per second. */
    float cruiseMaxSpeed = 30000.f;

    /** How quickly cruise speed builds, and how quickly it bleeds off, in u/s^2. */
    float cruiseAcceleration = 2500.f;
    float cruiseDeceleration = 9000.f;

    /** Cruise speed limit gained per world unit of clearance beyond the nearest mass-lock boundary. */
    float cruiseSlowdownRate = 0.5f;

    /**
     * Distance past the nearest mass-lock boundary, refreshed by World every physics step.
     * Zero or below means the ship is mass-locked: cruise is unavailable and drops out.
     */
    float cruiseMargin = 0.f;

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

/** Returns the ship's right direction before roll is applied (always horizontal). */
inline Vec3 shipUnrolledRight(const Ship& ship)
{
    return normalized(
    {
        std::cos(ship.yaw),
        0.f,
        -std::sin(ship.yaw)
    });
}

/** Returns the ship's up direction before roll is applied. */
inline Vec3 shipUnrolledUp(const Ship& ship)
{
    return normalized(cross(shipForward(ship), shipUnrolledRight(ship)));
}

/** Returns the ship's local right direction, rotated about the forward axis by roll. */
inline Vec3 shipRight(const Ship& ship)
{
    if (ship.roll == 0.f)
        return shipUnrolledRight(ship);

    return normalized(
        shipUnrolledRight(ship) * std::cos(ship.roll) +
        shipUnrolledUp(ship) * std::sin(ship.roll)
    );
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
