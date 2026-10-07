//
// Created by Mykyta Khomiakov on 26/07/2026.
//
// Simple-reflex NPC behaviour: pick a destination, steer toward it while avoiding bodies, use
// the cruise drive on long legs, dock at the station, and warp in and out of the system.
//

#ifndef DUSK_NPC_AI_H
#define DUSK_NPC_AI_H

#include "math/Vec3.h++"
#include "objects/npc_ship.h++"
#include "objects/planet.h++"
#include "objects/ship.h++"
#include "procgen/planet_generation.h++"
#include "systems/ship_physics.h++"
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace npc_ai {

constexpr float pi = 3.14159265358979323846f;

/** Throttle NPCs hold; with flight assist this is a speed demand (fraction of maxSpeed, or of cruiseMaxSpeed). */
constexpr float cruiseThrottle = 0.45f;
constexpr float turnRate = 0.6f; // radians per second
constexpr float arrivalRadius = 900.f;

/** NPCs use the cruise drive for any leg longer than this, and drop out once they get this close. */
constexpr float cruiseEngageDistance = 40000.f;
constexpr float cruiseDropDistance = 8000.f;

/** In cruise, NPCs aim for this many units per second for every unit of remaining distance, so they arrive rather than overshoot. */
constexpr float cruiseArrivalGain = 0.5f;

/** Within this distance of the target, normal-space throttle tapers off so the NPC lands inside the arrival radius. */
constexpr float slowdownDistance = 6000.f;

constexpr float minWarpOutDuration = 6.f;
constexpr float maxWarpOutDuration = 18.f;
/** How long an NPC stays inside the station before launching again, in seconds. */
constexpr float minDockDuration = 12.f;
constexpr float maxDockDuration = 35.f;

/** After reaching a roam waypoint: chance to head for the station, and chance to leave the system. */
constexpr float stationVisitChance = 0.55f;
constexpr float warpOutChance = 0.12f;

/** Chance that a ship arriving in the system appears launching from the station instead of in open space. */
constexpr float launchOnArrivalChance = 0.35f;

/* ---- Docking manoeuvre ---------------------------------------------------------------------- */

/** The approach point sits this far out from the slot's mouth along its normal. */
constexpr float dockApproachDistance = 2600.f;

/** How close to the approach point an NPC must get before lining up and entering. */
constexpr float dockApproachArrival = 700.f;

/** Speed down the slot axis while entering, and out of it while launching (relative to the station). */
constexpr float dockEntrySpeed = 380.f;
constexpr float dockLaunchSpeed = 340.f;

/** How far past the mouth the ship travels before it counts as inside (and disappears). */
constexpr float dockInsideDepth = 260.f;

/** A launching ship hands over to normal flight once it is this far out of the slot. */
constexpr float launchClearDistance = 3000.f;

/** Rates for lining up: how fast a ship turns its nose and rolls to the slot, and how fast its sideways offset closes. */
constexpr float dockTurnRate = 1.1f;
constexpr float dockRollRate = 1.2f;
constexpr float dockLateralClosing = 1.6f;

/** Within this distance of the approach point, NPCs only steer clear of planet surfaces, not their wide avoidance zones. */
constexpr float finalApproachRange = 25000.f;

/**
 * The station's docking slot in world space, refreshed every step by World and handed to each
 * NPC. Keeping this a plain struct means the AI doesn't need to know about Station or World.
 */
struct StationDockingInfo {
    bool exists = false;
    Vec3 mouth;       // centre of the slot opening
    Vec3 normal;      // pointing out of the station through the slot
    Vec3 slotAxis;    // the slot's long side; ships roll to line their wings up with it
    Vec3 velocity;    // the station's world velocity (it orbits a moving planet)

    /** False while the player holds the slot: NPCs don't start docking or launching until it's free again. */
    bool slotFree = true;
};

/** The point out in front of the slot that NPCs fly to before lining up. */
inline Vec3 dockApproachPoint(const StationDockingInfo& dock)
{
    return dock.mouth + dock.normal * dockApproachDistance;
}

/**
 * Roll that lines a ship's wings up with the slot's long side, given the ship's current yaw and
 * pitch. Uses the ship's roll convention (right = unrolledRight cos r + unrolledUp sin r). The
 * slot is symmetric, so of the two answers half a turn apart, the smaller one is returned.
 */
inline float slotRoll(const Ship& ship, const Vec3& slotAxis)
{
    const Vec3 forward = shipForward(ship);
    Vec3 wings = slotAxis - forward * dot(slotAxis, forward);

    if (length(wings) < 1e-4f)
        return 0.f;

    wings = normalized(wings);
    float roll = std::atan2(dot(wings, shipUnrolledUp(ship)), dot(wings, shipUnrolledRight(ship)));

    if (roll > pi * 0.5f)
        roll -= pi;
    else if (roll < -pi * 0.5f)
        roll += pi;

    return roll;
}

/** Turns nose and wings toward a docking attitude at the docking rates (pointing along `direction`, wings on the slot). */
inline void steerToDockingAttitude(Ship& ship, const Vec3& direction, const Vec3& slotAxis, float dt)
{
    const float desiredYaw = std::atan2(direction.x, direction.z);
    const float desiredPitch = std::asin(std::clamp(direction.y, -1.f, 1.f));
    const float maxTurn = dockTurnRate * dt;

    ship.yaw = wrapAngle(ship.yaw + std::clamp(wrapAngle(desiredYaw - ship.yaw), -maxTurn, maxTurn));
    ship.pitch = std::clamp(ship.pitch + std::clamp(desiredPitch - ship.pitch, -maxTurn, maxTurn), -1.25f, 1.25f);

    const float maxRoll = dockRollRate * dt;
    ship.roll += std::clamp(wrapAngle(slotRoll(ship, slotAxis) - ship.roll), -maxRoll, maxRoll);
}

/**
 * Places a ship on the slot axis: `dockDistance` out from the mouth plus its remaining sideways
 * offset, moving with the station plus `axisSpeed` along the normal. Docking and launching are
 * kinematic (like the player's docking computer), so a ship stays exactly on the moving,
 * spinning slot rather than chasing it.
 */
inline void placeOnSlotAxis(NpcShip& npc, const StationDockingInfo& dock, float axisSpeed)
{
    npc.ship.previousPosition = npc.ship.position;
    npc.ship.position = dock.mouth + dock.normal * npc.dockDistance + npc.dockLateral;
    npc.ship.velocity = dock.velocity + dock.normal * axisSpeed;
    npc.ship.cruiseEngaged = false;
}

/** Starts a launch: the ship appears just inside the slot, nose out, wings on the slot. */
inline void beginLaunch(NpcShip& npc, const StationDockingInfo& dock)
{
    npc.state = NpcState::Launching;
    npc.stateTimer = 0.f;
    npc.dockDistance = -dockInsideDepth;
    npc.dockLateral = {};
    npc.ship.yaw = std::atan2(dock.normal.x, dock.normal.z);
    npc.ship.pitch = std::asin(std::clamp(dock.normal.y, -1.f, 1.f));
    npc.ship.roll = slotRoll(npc.ship, dock.slotAxis);
    placeOnSlotAxis(npc, dock, dockLaunchSpeed);
}

/** Returns a random point on the surface of a sphere with the given radius. */
inline Vec3 randomPointOnSphere(std::mt19937& rng, float radius)
{
    std::uniform_real_distribution<float> angleDist(0.f, 2.f * pi);
    std::uniform_real_distribution<float> heightDist(-1.f, 1.f);

    const float theta = angleDist(rng);
    const float height = heightDist(rng);
    const float ringRadius = std::sqrt(std::max(0.f, 1.f - height * height));

    return
    {
        ringRadius * std::cos(theta) * radius,
        height * radius,
        ringRadius * std::sin(theta) * radius
    };
}

/** Picks a random point inside the system, then nudges it clear of whichever body it lands nearest. */
inline Vec3 pickSafeSystemPoint(
    std::mt19937& rng,
    const Planet& star,
    const std::vector<Planet>& planets,
    float minRadius,
    float maxRadius
)
{
    std::uniform_real_distribution<float> radiusDist(minRadius, maxRadius);
    const Vec3 candidate = randomPointOnSphere(rng, radiusDist(rng));

    const Planet* nearest = procgen::findNearestBody(candidate, star, planets);
    const float requiredDistance = nearest->radius * 1.2f;
    const float currentDistance = length(candidate - nearest->position);

    if (currentDistance >= requiredDistance)
        return candidate;

    const Vec3 direction = currentDistance > 0.f
        ? normalized(candidate - nearest->position)
        : Vec3{0.f, 0.f, -1.f};

    return nearest->position + direction * requiredDistance;
}

/**
 * Returns a steering direction toward `target`, bent away from any body the ship is
 * currently too close to. This is the "reflex" part: the decision only depends on the
 * ship's current position and its immediate surroundings, not any remembered state.
 */
inline Vec3 desiredDirection(
    const Vec3& position,
    const Vec3& target,
    const Planet& star,
    const std::vector<Planet>& planets,
    bool finalApproach = false
)
{
    const Vec3 toTarget = target - position;
    const Vec3 seek = length(toTarget) > 0.f ? normalized(toTarget) : Vec3{0.f, 0.f, 1.f};

    Vec3 avoidance{};

    const auto accumulateAvoidance = [&](const Planet& body)
    {
        const Vec3 offset = position - body.position;
        const float distance = length(offset);
        // Scaled for large bodies: a giant's avoidance zone shouldn't swallow half its orbit. On
        // the final approach to a station, which orbits well inside that zone, only the
        // surface itself is avoided, or the planet would push its own visitors away.
        const float safeDistance = finalApproach
            ? body.radius * 1.06f + 1500.f
            : body.radius * 1.5f + 6000.f;

        if (distance < safeDistance && distance > 0.f)
        {
            const float strength = (safeDistance - distance) / safeDistance;
            avoidance += normalized(offset) * strength;
        }
    };

    accumulateAvoidance(star);

    for (const Planet& planet : planets)
        accumulateAvoidance(planet);

    // Avoidance dominates the moment a hazard gets close, so ships peel away rather than clip in.
    const Vec3 combined = seek + avoidance * 3.f;
    return length(combined) > 0.f ? normalized(combined) : seek;
}

/** Rotates a ship's yaw/pitch toward a direction at a limited turn rate, instead of snapping to it. */
inline void steerToward(Ship& ship, const Vec3& direction, float dt)
{
    const float desiredYaw = std::atan2(direction.x, direction.z);
    const float desiredPitch = std::asin(std::clamp(direction.y, -1.f, 1.f));
    const float maxStep = turnRate * dt;

    const float yawDelta = std::clamp(wrapAngle(desiredYaw - ship.yaw), -maxStep, maxStep);
    const float pitchDelta = std::clamp(desiredPitch - ship.pitch, -maxStep, maxStep);

    ship.yaw = wrapAngle(ship.yaw + yawDelta);
    ship.pitch = std::clamp(ship.pitch + pitchDelta, -1.25f, 1.25f);
}

/** Places an NPC at a safe point and starts it roaming toward a fresh waypoint — the "warp in". */
inline void respawnNpc(
    NpcShip& npc,
    std::mt19937& rng,
    const Planet& star,
    const std::vector<Planet>& planets,
    float systemOuterRadius,
    const StationDockingInfo& dock = {}
)
{
    npc.ship.position = pickSafeSystemPoint(rng, star, planets, star.radius * 3.f, systemOuterRadius);
    npc.ship.previousPosition = npc.ship.position;
    npc.ship.velocity = {};
    npc.ship.throttle = cruiseThrottle;
    npc.ship.reverseThrust = false;
    npc.ship.cruiseEngaged = false;

    npc.roamTarget = pickSafeSystemPoint(rng, star, planets, star.radius * 3.f, systemOuterRadius);
    npc.state = NpcState::Roaming;
    npc.stateTimer = 0.f;
    npc.ship.roll = 0.f;

    // Some arrivals come out of the station instead, so it's visibly busy.
    std::uniform_real_distribution<float> chance(0.f, 1.f);

    if (dock.exists && dock.slotFree && chance(rng) < launchOnArrivalChance)
        beginLaunch(npc, dock);
}

/**
 * Advances one NPC's behaviour and physics by one step.
 *
 *  Inactive          wait out wakeDelay, then respawn (sometimes launching from the station)
 *  Roaming           fly to a waypoint; on arrival, maybe head for the station or warp out
 *  HeadingToStation  fly to the approach point in front of the slot
 *  EnteringStation   line up, roll to the slot, fly down its axis; disappear once inside
 *  Docked            wait inside (invisible), then launch
 *  Launching         fly out along the slot axis, then hand over to Roaming
 *  WarpingOut        a brief wind-up, then vanish
 */
inline void updateNpcShip(
    NpcShip& npc,
    float dt,
    std::mt19937& rng,
    const Planet& star,
    const std::vector<Planet>& planets,
    const StationDockingInfo& dock,
    float systemOuterRadius,
    const Vec3& gravity
)
{
    npc.stateTimer += dt;

    if (npc.state == NpcState::Inactive)
    {
        if (npc.stateTimer >= npc.wakeDelay)
            respawnNpc(npc, rng, star, planets, systemOuterRadius, dock);
        return;
    }

    // Without a station, anything bound for one goes back to roaming.
    if (!dock.exists && (npc.state == NpcState::HeadingToStation ||
                         npc.state == NpcState::EnteringStation ||
                         npc.state == NpcState::Launching ||
                         npc.state == NpcState::Docked))
    {
        npc.state = NpcState::Roaming;
        npc.roamTarget = pickSafeSystemPoint(rng, star, planets, star.radius * 3.f, systemOuterRadius);
    }

    if (npc.state == NpcState::Docked)
    {
        // Waits inside until the slot is free (the player may be docking or launching).
        if (npc.stateTimer >= npc.dockDuration && dock.slotFree)
            beginLaunch(npc, dock);
        return;
    }

    if (npc.state == NpcState::EnteringStation)
    {
        // Close the sideways gap and turn nose-in while moving down the axis.
        npc.dockLateral = npc.dockLateral * std::exp(-dockLateralClosing * dt);
        npc.dockDistance -= dockEntrySpeed * dt;
        steerToDockingAttitude(npc.ship, dock.normal * -1.f, dock.slotAxis, dt);
        placeOnSlotAxis(npc, dock, -dockEntrySpeed);

        if (npc.dockDistance <= -dockInsideDepth)
        {
            npc.state = NpcState::Docked;
            npc.stateTimer = 0.f;
            std::uniform_real_distribution<float> dockDist(minDockDuration, maxDockDuration);
            npc.dockDuration = dockDist(rng);
        }

        return;
    }

    if (npc.state == NpcState::Launching)
    {
        npc.dockDistance += dockLaunchSpeed * dt;
        steerToDockingAttitude(npc.ship, dock.normal, dock.slotAxis, dt);
        placeOnSlotAxis(npc, dock, dockLaunchSpeed);

        if (npc.dockDistance >= launchClearDistance)
        {
            npc.state = NpcState::Roaming;
            npc.stateTimer = 0.f;
            npc.roamTarget = pickSafeSystemPoint(rng, star, planets, star.radius * 3.f, systemOuterRadius);
            npc.ship.throttle = cruiseThrottle;
        }

        return;
    }

    if (npc.state == NpcState::WarpingOut)
    {
        if (npc.stateTimer >= 1.f)
        {
            npc.state = NpcState::Inactive;
            npc.stateTimer = 0.f;
            std::uniform_real_distribution<float> delayDist(minWarpOutDuration, maxWarpOutDuration);
            npc.wakeDelay = delayDist(rng);
            return;
        }

        // Still flies its current heading during the brief wind-up before vanishing.
    }

    const bool toStation = npc.state == NpcState::HeadingToStation;
    const Vec3 target = toStation ? dockApproachPoint(dock) : npc.roamTarget;
    const bool finalApproach = toStation && length(target - npc.ship.position) < finalApproachRange;
    const Vec3 direction = desiredDirection(npc.ship.position, target, star, planets, finalApproach);
    steerToward(npc.ship, direction, dt);

    // Level the wings again after a launch.
    npc.ship.roll *= std::exp(-1.5f * dt);

    // Long legs go by cruise drive; the physics step drops it automatically inside mass-lock zones.
    const float distanceAhead = length(target - npc.ship.position);

    if (!npc.ship.cruiseEngaged && distanceAhead > cruiseEngageDistance)
        engageCruise(npc.ship);
    else if (npc.ship.cruiseEngaged && distanceAhead < cruiseDropDistance)
        disengageCruise(npc.ship);

    if (npc.ship.cruiseEngaged)
    {
        const float arrivalThrottle = distanceAhead * cruiseArrivalGain / npc.ship.cruiseMaxSpeed;
        npc.ship.throttle = std::min(cruiseThrottle, arrivalThrottle);
    }
    else
    {
        const float taper = std::clamp(distanceAhead / slowdownDistance, 0.25f, 1.f);
        npc.ship.throttle = cruiseThrottle * taper;
    }

    integrateShipPhysics(npc.ship, dt, gravity);

    if (npc.state == NpcState::WarpingOut)
        return; // don't re-evaluate arrival/state transitions during wind-up

    const float distanceToTarget = length(target - npc.ship.position);

    if (toStation)
    {
        // At the approach point: line up and go in, once the slot is free. Until then it loiters
        // around the approach point (it keeps flying, so it circles rather than hovering).
        if (distanceToTarget <= dockApproachArrival && dock.slotFree)
        {
            const Vec3 offset = npc.ship.position - dock.mouth;
            npc.dockDistance = dot(offset, dock.normal);
            npc.dockLateral = offset - dock.normal * npc.dockDistance;
            npc.state = NpcState::EnteringStation;
            npc.stateTimer = 0.f;
            disengageCruise(npc.ship);
        }

        return;
    }

    if (distanceToTarget > arrivalRadius)
        return;

    std::uniform_real_distribution<float> chanceDist(0.f, 1.f);
    const float roll = chanceDist(rng);

    if (dock.exists && roll < stationVisitChance)
    {
        npc.state = NpcState::HeadingToStation;
    }
    else if (roll < stationVisitChance + warpOutChance)
    {
        npc.state = NpcState::WarpingOut;
        npc.stateTimer = 0.f;
    }
    else
    {
        npc.roamTarget = pickSafeSystemPoint(rng, star, planets, star.radius * 3.f, systemOuterRadius);
    }
}

} // namespace npc_ai

#endif //DUSK_NPC_AI_H