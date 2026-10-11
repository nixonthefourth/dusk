//
// Mining: the mining lasers cut ore from rocks and the fragments fly into the mining hatch.
//
// While the lasers fire, a ray from between the muzzles along the ship's nose finds the nearest
// rock within range. Ore is cut at a steady rate; every fragmentTonnes cut breaks off as a
// fragment that is flung outward from the impact point and then pulled into the hatch, where its
// tonnes are added when it arrives. A rock holds a finite amount (see rockCapacity()) and shrinks
// as it is mined; the hatch holds miningHatchCapacity tonnes.
//
// Everything here is plain logic over the World and the hatch, with no drawing, so it can be
// tested headless. The scene decides whether the lasers may fire at all (see SystemScene).
//

#ifndef DUSK_MINING_H
#define DUSK_MINING_H

#include "objects/asteroid.h++"
#include "objects/hardpoints.h++"
#include "objects/mining_hatch.h++"
#include "objects/ship.h++"
#include "procgen/asteroid_generation.h++"
#include "world/mining_state.h++"
#include "world/world.h++"
#include <algorithm>
#include <cmath>
#include <optional>

namespace mining {

    /** How far the mining lasers reach, in world units. */
    constexpr float laserRange = 1500.f;

    /** Tonnes of ore cut per second while the lasers hold a rock: an empty hatch fills in about half a minute. */
    constexpr float extractionRate = 0.15f;

    /** Ore breaks off in fragments of this many tonnes (so about three a second). */
    constexpr float fragmentTonnes = 0.05f;

    /** Seconds a fragment takes to fly from the rock into the hatch. */
    constexpr float fragmentFlightSeconds = 1.2f;

    /** Seconds for the beam to come up to full brightness, or fade out. */
    constexpr float beamRampSeconds = 0.15f;

    /** Lasers can't be fired within this distance of a station (Elite's no-fire zone). */
    constexpr float noFireZoneRadius = 5000.f;

    /** The largest rock radius there is, plus room: how far beyond the laser's reach a rock's centre can be and still be hit. */
    constexpr float largestRockReach = 2700.f;

    /** Where the hatch opens, on the underside of the ship, in its local frame. Fragments fly to it. */
    inline const Vec3 hatchMount{0.f, -48.f, 30.f};

    /** What a laser ray hit: the rock (as generated, unshrunk, with its id), how far along the ray, and where. */
    struct LaserHit {
        Asteroid rock;
        float distance = 0.f;
        Vec3 point;
        Vec3 normal;
    };

    /** Distance along a unit ray to where it enters a sphere, if it does within `range`. */
    inline std::optional<float> raySphere(const Vec3& origin, const Vec3& direction, const Vec3& centre, float radius, float range)
    {
        const Vec3 toCentre = centre - origin;
        const float along = dot(toCentre, direction);
        const float missSquared = dot(toCentre, toCentre) - along * along;
        const float radiusSquared = radius * radius;

        if (missSquared > radiusSquared)
            return std::nullopt;

        const float halfChord = std::sqrt(radiusSquared - missSquared);
        float entry = along - halfChord;

        // Starting inside the rock (pressed up against it): it is hit right away.
        if (entry < 0.f)
            entry = along + halfChord >= 0.f ? 0.f : -1.f;

        if (entry < 0.f || entry > range)
            return std::nullopt;

        return entry;
    }

    /**
     * The nearest rock a laser fired from `origin` along unit `direction` hits within `range`, belt
     * rocks and drifting rocks alike. Rocks are tested at their current (shrunk) size and their
     * collision radius, the same sphere the ship bumps into.
     */
    inline std::optional<LaserHit> findLaserTarget(const World& world, const Vec3& origin, const Vec3& direction, float range = laserRange)
    {
        std::optional<LaserHit> best;

        const auto consider = [&](const Asteroid& rock)
        {
            const Asteroid shown = world.miningLedger.shrunk(rock);
            const auto entry = raySphere(origin, direction, shown.position, shown.collisionRadius(), range);

            if (!entry || (best && *entry >= best->distance))
                return;

            LaserHit hit;
            hit.rock = rock;
            hit.distance = *entry;
            hit.point = origin + direction * *entry;
            const Vec3 outward = hit.point - shown.position;
            hit.normal = length(outward) > 1e-3f ? normalized(outward) : direction * -1.f;
            best = hit;
        };

        // Search a sphere that covers the whole ray, plus the largest rock radius.
        const Vec3 middle = origin + direction * (range * 0.5f);

        for (const AsteroidBelt& belt : world.asteroidBelts)
            procgen::forEachAsteroidNear(belt, middle, range * 0.5f + largestRockReach, consider);

        for (const Asteroid& rock : world.driftingAsteroids)
        {
            if (length(rock.position - middle) <= range * 0.5f + rock.radius)
                consider(rock);
        }

        return best;
    }

    /** A cheap random number in [0, 1) from the mining state's own generator (xorshift32). */
    inline float nextRandom(MiningState& state)
    {
        std::uint32_t x = state.random;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state.random = x;
        return static_cast<float>(x >> 8) / 16777216.f;
    }

    /** Where a fragment is at its current age: thrown outward from the rock, then curving into the (moving) hatch. */
    inline Vec3 fragmentPosition(const MiningFragment& fragment, const Vec3& hatch)
    {
        const float t = std::clamp(fragment.age / fragment.duration, 0.f, 1.f);
        const float s = t * t * (3.f - 2.f * t);
        const Vec3 origin = fragment.start + fragment.startVelocity * fragment.age;
        const Vec3 control = origin + fragment.kick;
        const float u = 1.f - s;
        return origin * (u * u) + control * (2.f * u * s) + hatch * (s * s);
    }

    /** Something worth telling the pilot that happened this step. */
    enum class MiningEvent { None, HatchFilled, RockDepleted };

    /**
     * One step of mining. `fire` is whether the lasers may fire and the pilot is holding the
     * trigger (the scene has already checked the module, the selected hardpoint and the no-fire
     * zone). Moves the beam, cuts ore from the rock it holds, breaks off fragments, flies them in
     * and drops them into `hatch`. Fragments already flying keep coming after the trigger is let
     * go. Returns a HatchFilled or RockDepleted event on the step one of those starts.
     */
    inline MiningEvent updateMining(World& world, MiningHatch& hatch, bool fire, float dt)
    {
        MiningState& state = world.mining;
        const Ship& ship = world.playerShip;
        const LaserStatus statusBefore = state.status;
        const auto muzzles = hardpointMuzzles(HardpointKind::MiningLaser);
        const Vec3 origin = shipLocalToWorld(ship, (muzzles[0] + muzzles[1]) * 0.5f);
        const Vec3 forward = shipForward(ship);
        const float ramp = dt / beamRampSeconds;

        state.firing = fire;
        state.beamPower = std::clamp(state.beamPower + (fire ? ramp : -ramp), 0.f, 1.f);

        if (!fire)
        {
            state.status = LaserStatus::Off;
            state.hitting = false;
            state.accumulator = 0.f;
        }
        else
        {
            const auto hit = findLaserTarget(world, origin, forward);
            state.hitting = hit.has_value();
            state.beamEnd = hit ? hit->point : origin + forward * laserRange;

            if (!hit)
            {
                state.status = LaserStatus::NoTarget;
                state.accumulator = 0.f;
            }
            else
            {
                const Asteroid& rock = hit->rock;
                state.hitNormal = hit->normal;
                state.targetId = rock.id;
                state.targetComposition = rockComposition(rock.id);

                float remaining = world.miningLedger.remaining(rock);
                const float room = miningHatchCapacity - hatch.total() - state.inFlightTonnes();

                if (remaining <= 1e-4f)
                    state.status = LaserStatus::Depleted;
                else if (room < 1e-4f)
                    state.status = LaserStatus::HatchFull;
                else
                {
                    state.status = LaserStatus::Mining;
                    state.accumulator += extractionRate * dt;

                    // Break off fragments as ore accumulates: never more than the rock holds or the hatch has room for.
                    float space = room;

                    while (state.accumulator >= fragmentTonnes && remaining > 1e-4f && space > 1e-4f)
                    {
                        const float tonnes = std::min({fragmentTonnes, remaining, space});
                        state.accumulator -= fragmentTonnes;
                        remaining -= tonnes;
                        space -= tonnes;
                        world.miningLedger.record(rock.id, tonnes);

                        MiningFragment fragment;
                        fragment.resource = nextRandom(state) < mainResourceShare ? state.targetComposition.main : state.targetComposition.secondary;
                        fragment.tonnes = tonnes;
                        fragment.start = hit->point;
                        fragment.startVelocity = rock.velocity;

                        // Flung sideways along the rock's surface, and a little outward. (Thrown straight
                        // out, they would fly back along the line of sight from a chase camera, hidden in
                        // the impact flash; sideways, they arc across open space on their way in.)
                        const Vec3 scatter{nextRandom(state) - 0.5f, nextRandom(state) - 0.5f, nextRandom(state) - 0.5f};
                        Vec3 tangent = scatter - hit->normal * dot(scatter, hit->normal);

                        if (length(tangent) < 1e-3f)
                            tangent = cross(hit->normal, Vec3{0.f, 1.f, 0.f});

                        if (length(tangent) < 1e-3f)
                            tangent = cross(hit->normal, Vec3{1.f, 0.f, 0.f});

                        fragment.kick = (normalized(tangent) + hit->normal * 0.35f) * (240.f + 220.f * nextRandom(state));
                        fragment.duration = fragmentFlightSeconds * (0.85f + 0.3f * nextRandom(state));
                        fragment.spin = nextRandom(state) * 6.2832f;
                        fragment.position = fragment.start;
                        state.fragments.push_back(fragment);
                    }

                    if (remaining <= 1e-4f)
                        state.status = LaserStatus::Depleted;
                    else if (space <= 1e-4f)
                        state.status = LaserStatus::HatchFull;
                }

                state.targetRemainingTonnes = std::max(0.f, remaining);
                state.targetRemainingFraction = rockCapacity(rock) > 0.f ? state.targetRemainingTonnes / rockCapacity(rock) : 0.f;
            }
        }

        // Fly the fragments in; each one's ore goes into the hatch when it arrives.
        const Vec3 hatchPoint = shipLocalToWorld(ship, hatchMount);

        for (MiningFragment& fragment : state.fragments)
        {
            fragment.age += dt;
            fragment.position = fragmentPosition(fragment, hatchPoint);
        }

        for (const MiningFragment& fragment : state.fragments)
        {
            if (fragment.age >= fragment.duration)
                hatch.add(fragment.resource, std::min(fragment.tonnes, std::max(0.f, miningHatchCapacity - hatch.total())));
        }

        std::erase_if(state.fragments, [](const MiningFragment& fragment) { return fragment.age >= fragment.duration; });

        if (state.status != statusBefore)
        {
            if (state.status == LaserStatus::HatchFull)
                return MiningEvent::HatchFilled;

            if (state.status == LaserStatus::Depleted)
                return MiningEvent::RockDepleted;
        }

        return MiningEvent::None;
    }

    /** The mining status as a short line for the HUD. */
    inline const char* laserStatusLabel(LaserStatus status)
    {
        switch (status)
        {
            case LaserStatus::Off: return "READY";
            case LaserStatus::NoTarget: return "NO ROCK IN RANGE";
            case LaserStatus::Mining: return "MINING";
            case LaserStatus::HatchFull: return "HATCH FULL";
            case LaserStatus::Depleted: return "ROCK DEPLETED";
        }

        return "";
    }

} // namespace mining

#endif //DUSK_MINING_H
