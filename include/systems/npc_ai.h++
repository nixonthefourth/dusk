//
// Created by Mykyta Khomiakov on 26/07/2026.
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

constexpr float cruiseThrottle = 0.45f;
constexpr float turnRate = 0.6f; // radians per second
constexpr float arrivalRadius = 900.f;

constexpr float minWarpOutDuration = 6.f;
constexpr float maxWarpOutDuration = 18.f;
constexpr float minDockDuration = 4.f;
constexpr float maxDockDuration = 10.f;

constexpr float stationVisitChance = 0.25f;
constexpr float warpOutChance = 0.15f;

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
    const std::vector<Planet>& planets
)
{
    const Vec3 toTarget = target - position;
    const Vec3 seek = length(toTarget) > 0.f ? normalized(toTarget) : Vec3{0.f, 0.f, 1.f};

    Vec3 avoidance{};

    const auto accumulateAvoidance = [&](const Planet& body)
    {
        const Vec3 offset = position - body.position;
        const float distance = length(offset);
        const float safeDistance = body.radius * 2.5f;

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
    float systemOuterRadius
)
{
    npc.ship.position = pickSafeSystemPoint(rng, star, planets, star.radius * 3.f, systemOuterRadius);
    npc.ship.previousPosition = npc.ship.position;
    npc.ship.velocity = {};
    npc.ship.throttle = cruiseThrottle;
    npc.ship.reverseThrust = false;

    npc.roamTarget = pickSafeSystemPoint(rng, star, planets, star.radius * 3.f, systemOuterRadius);
    npc.state = NpcState::Roaming;
    npc.stateTimer = 0.f;
}

/** Advances one NPC's simple-reflex behaviour and physics by one frame. */
inline void updateNpcShip(
    NpcShip& npc,
    float dt,
    std::mt19937& rng,
    const Planet& star,
    const std::vector<Planet>& planets,
    const Vec3& stationPosition,
    bool stationExists,
    float systemOuterRadius,
    const Vec3& gravity
)
{
    npc.stateTimer += dt;

    if (npc.state == NpcState::Inactive)
    {
        if (npc.stateTimer >= npc.wakeDelay)
            respawnNpc(npc, rng, star, planets, systemOuterRadius);

        return;
    }

    if (npc.state == NpcState::Docked)
    {
        if (npc.stateTimer >= npc.dockDuration)
        {
            npc.state = NpcState::Roaming;
            npc.stateTimer = 0.f;
            npc.roamTarget = pickSafeSystemPoint(rng, star, planets, star.radius * 3.f, systemOuterRadius);
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

    const Vec3 target = npc.state == NpcState::HeadingToStation ? stationPosition : npc.roamTarget;
    const Vec3 direction = desiredDirection(npc.ship.position, target, star, planets);

    steerToward(npc.ship, direction, dt);
    npc.ship.throttle = cruiseThrottle;

    integrateShipPhysics(npc.ship, dt, gravity);

    if (npc.state == NpcState::WarpingOut)
        return; // don't re-evaluate arrival/state transitions during wind-up

    const float distanceToTarget = length(target - npc.ship.position);

    if (distanceToTarget > arrivalRadius)
        return;

    if (npc.state == NpcState::HeadingToStation)
    {
        npc.state = NpcState::Docked;
        npc.stateTimer = 0.f;

        std::uniform_real_distribution<float> dockDist(minDockDuration, maxDockDuration);
        npc.dockDuration = dockDist(rng);
        return;
    }

    std::uniform_real_distribution<float> chanceDist(0.f, 1.f);
    const float roll = chanceDist(rng);

    if (stationExists && roll < stationVisitChance)
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