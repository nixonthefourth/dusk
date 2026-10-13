//
// Combat: the four weapon hardpoints, what they hit, and what happens to the ships they hit.
//
//   Pulse laser      fixed, forward; fast bolts from alternating wing mounts
//   Beam laser       fixed, forward; a continuous beam that does the most damage and drains the
//                    capacitor fastest
//   Rocket launcher  rockets from the wing pods; they home on the locked ship if it is roughly ahead
//                    when they are launched, and burst with splash damage
//   Turret laser     on top of the hull; aims itself at the locked ship (leading it), within its arc
//
// The lasers draw on a shared capacitor (CombatState::energy) that recharges continuously; rockets
// are limited by ammunition instead. NPC ships have shields, which take damage first and recharge
// after a pause, and hull. A ship whose shields fail runs and warps out a few seconds later; a ship
// whose hull reaches zero explodes, and the system's traffic brings a new ship in later.
//
// Everything here is plain logic over the World with no drawing, so it can be tested headless. The
// scene decides whether a weapon may fire at all (no-fire zone, cruise, docking...).
//

#ifndef DUSK_COMBAT_H
#define DUSK_COMBAT_H

#include "objects/hardpoints.h++"
#include "objects/npc_ship.h++"
#include "objects/ship.h++"
#include "systems/mining.h++"
#include "world/combat_state.h++"
#include "world/world.h++"
#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace combat {

    /* ---- Tuning ------------------------------------------------------------------------------ */

    /** The capacitor recharges this much energy a second. */
    constexpr float energyRecharge = 18.f;

    /** Pulse laser: seconds between bolts, energy per bolt, damage per bolt, bolt speed and reach. */
    constexpr float pulseInterval = 0.12f;
    constexpr float pulseEnergy = 2.f;
    constexpr float pulseDamage = 4.f;
    constexpr float pulseSpeed = 3200.f;
    constexpr float pulseRange = 2600.f;

    /** Beam laser: damage and energy per second at full power, reach, and seconds to ramp up or down. */
    constexpr float beamDamagePerSecond = 42.f;
    constexpr float beamEnergyPerSecond = 38.f;
    constexpr float beamRange = 2200.f;
    constexpr float beamRampSeconds = 0.12f;

    /** Rocket launcher: rockets carried, seconds between launches, damage, splash radius and price per rocket at a station. */
    constexpr int rocketCapacity = rocketMagazine;
    constexpr float rocketInterval = 0.55f;
    constexpr float rocketDamage = 55.f;
    constexpr float rocketSplashRadius = 260.f;
    constexpr double rocketPrice = 30.0;

    /** Rocket flight: launch speed, own top speed, acceleration, turn rate, fuel, proximity fuse. */
    constexpr float rocketLaunchSpeed = 220.f;
    constexpr float rocketSpeed = 1700.f;
    constexpr float rocketAcceleration = 1400.f;
    constexpr float rocketTurnRate = 2.6f;
    constexpr float rocketLife = 6.5f;
    constexpr float rocketFuse = 140.f;

    /** A rocket homes on the locked ship only if it is within this range and this cone of the nose when launched. */
    constexpr float rocketLockRange = 7000.f;
    constexpr float rocketLockCone = 0.82f; // cosine of about 35 degrees

    /** Turret laser: seconds between bolts, energy per bolt, damage per bolt, bolt speed, reach, turn rate, and how close its aim must be to fire. */
    constexpr float turretInterval = 0.18f;
    constexpr float turretEnergy = 1.3f;
    constexpr float turretDamage = 3.f;
    constexpr float turretSpeed = 2900.f;
    constexpr float turretRange = 2300.f;
    constexpr float turretSlewRate = 2.2f;
    constexpr float turretOnTargetCos = 0.9986f; // about 3 degrees

    /** The turret sits on top of the hull, so it can't fire much below the ship's own plane. */
    constexpr float turretArcFloor = -0.2f;

    /** NPC ships: how big a target they are, seconds before shields recharge, and how fast. */
    constexpr float npcHitRadius = 230.f;
    constexpr float shieldRechargeDelay = 5.f;
    constexpr float shieldRechargeRate = 6.f;

    /** Seconds a ship whose shields have failed runs before it warps out. */
    constexpr float fleeSeconds = 3.5f;

    /** The player's ship: how big a target it is, and its shield recharge (seconds of quiet first, then points a second). */
    constexpr float playerHitRadius = 190.f;
    constexpr float playerShieldDelay = 4.f;
    constexpr float playerShieldRate = 5.f;

    /** Bounty paid for each pirate destroyed (collected on docking). */
    constexpr double pirateBounty = 150.0;

    /** Upper bound on live particles, so a long fight can't grow them without limit. */
    constexpr std::size_t particleLimit = 2400;

    /* ---- Events ------------------------------------------------------------------------------ */

    /** Something worth telling the pilot. */
    enum class CombatEventKind { ShipDestroyed, TargetDestroyed, ShieldsDown, OutOfRockets, PirateDestroyed, PlayerHit, PlayerDestroyed, PiratesArrived, PirateFled, BountyTargetsSighted };

    /** An event, and the NPC ship it concerns (or -1). */
    struct CombatEvent {
        CombatEventKind kind;
        int ship = -1;
    };

    /* ---- Small helpers ----------------------------------------------------------------------- */

    /** A random number in [0, 1) from the combat state's own generator (xorshift32). */
    inline float nextRandom(CombatState& state)
    {
        std::uint32_t x = state.random;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state.random = x;
        return static_cast<float>(x >> 8) / 16777216.f;
    }

    /** A random unit vector. */
    inline Vec3 randomDirection(CombatState& state)
    {
        for (int attempt = 0; attempt < 8; ++attempt)
        {
            const Vec3 v{nextRandom(state) * 2.f - 1.f, nextRandom(state) * 2.f - 1.f, nextRandom(state) * 2.f - 1.f};
            const float l = length(v);

            if (l > 0.05f && l <= 1.f)
                return v / l;
        }

        return {0.f, 1.f, 0.f};
    }

    /** Adds a particle, dropping the oldest if the limit is reached. */
    inline void addParticle(CombatState& state, const Particle& particle)
    {
        if (state.particles.size() >= particleLimit)
            state.particles.erase(state.particles.begin());

        state.particles.push_back(particle);
    }

    /** A spray of sparks from `point`, mostly along `normal`, carried along at `carrier` velocity. */
    inline void spawnSparks(CombatState& state, const Vec3& point, const Vec3& normal, const Vec3& carrier, int count, float speed)
    {
        for (int i = 0; i < count; ++i)
        {
            Particle spark;
            spark.kind = ParticleKind::Spark;
            spark.position = point;
            spark.velocity = carrier + normalized(normal + randomDirection(state) * 0.9f) * (speed * (0.4f + 0.6f * nextRandom(state)));
            spark.life = 0.25f + 0.35f * nextRandom(state);
            spark.size = 1.f;
            addParticle(state, spark);
        }
    }

    /** A muzzle flash: a brief bright particle that rides with the ship. */
    inline void spawnFlash(CombatState& state, const Vec3& point, const Vec3& carrier, float size)
    {
        Particle flash;
        flash.kind = ParticleKind::Flash;
        flash.position = point;
        flash.velocity = carrier;
        flash.life = 0.07f;
        flash.size = size;
        addParticle(state, flash);
    }

    /** An explosion at `position`: a flash and shock ring, sparks, and (for a ship) tumbling debris. */
    inline void spawnExplosion(CombatState& state, const Vec3& position, const Vec3& velocity, float radius, int debris)
    {
        Explosion explosion;
        explosion.position = position;
        explosion.velocity = velocity;
        explosion.radius = radius;
        explosion.life = 0.6f + radius / 900.f;
        state.explosions.push_back(explosion);

        spawnSparks(state, position, randomDirection(state), velocity, 10 + debris / 2, radius * 1.6f);

        for (int i = 0; i < debris; ++i)
        {
            Particle piece;
            piece.kind = ParticleKind::Debris;
            piece.position = position + randomDirection(state) * (radius * 0.15f);
            piece.velocity = velocity + randomDirection(state) * (radius * (0.3f + 0.9f * nextRandom(state)));
            piece.life = 1.4f + 1.6f * nextRandom(state);
            piece.size = 6.f + 18.f * nextRandom(state);
            piece.spin = nextRandom(state) * 6.2832f;
            addParticle(state, piece);
        }
    }

    /* ---- What a weapon hits ------------------------------------------------------------------ */

    /** What a ray (a beam, or a bolt's path over one step) hit first. */
    struct WeaponHit {
        enum class Kind { None, Ship, Rock, Station } kind = Kind::None;
        int ship = -1;
        float distance = 0.f;
        Vec3 point;
        Vec3 normal;
    };

    /** Distance along a unit ray to the first station triangle it crosses within `range`, if any (Moller-Trumbore, in the station's frame). */
    inline std::optional<float> rayStation(const Station& station, const Vec3& origin, const Vec3& direction, float range)
    {
        if (station.model.faces.empty())
            return std::nullopt;

        const Vec3 localOrigin = stationWorldToLocal(station, origin);
        const Vec3 localDirection{dot(direction, station.axisX), dot(direction, station.axisY), dot(direction, station.axisZ)};
        std::optional<float> nearest;

        for (const VectorFace& face : station.model.faces)
        {
            const Vec3& a = station.model.vertices[static_cast<std::size_t>(face.a)];
            const Vec3 edge1 = station.model.vertices[static_cast<std::size_t>(face.b)] - a;
            const Vec3 edge2 = station.model.vertices[static_cast<std::size_t>(face.c)] - a;
            const Vec3 pvec = cross(localDirection, edge2);
            const float determinant = dot(edge1, pvec);

            if (std::abs(determinant) < 1e-9f)
                continue;

            const float inverse = 1.f / determinant;
            const Vec3 tvec = localOrigin - a;
            const float u = dot(tvec, pvec) * inverse;

            if (u < 0.f || u > 1.f)
                continue;

            const Vec3 qvec = cross(tvec, edge1);
            const float v = dot(localDirection, qvec) * inverse;

            if (v < 0.f || u + v > 1.f)
                continue;

            const float t = dot(edge2, qvec) * inverse;

            if (t > 0.f && t <= range && (!nearest || t < *nearest))
                nearest = t;
        }

        return nearest;
    }

    /**
     * The first thing a ray from `origin` along unit `direction` hits within `range`: a visible NPC
     * ship, a rock (at its mined size), or the station's hull. The player's own ship is never hit.
     */
    inline WeaponHit castWeaponRay(const World& world, const Vec3& origin, const Vec3& direction, float range, bool includeShips = true)
    {
        WeaponHit best;
        float bestDistance = range;

        for (int i = 0; includeShips && i < static_cast<int>(world.npcShips.size()); ++i)
        {
            const NpcShip& npc = world.npcShips[static_cast<std::size_t>(i)];

            if (!npc.isVisible())
                continue;

            if (const auto t = mining::raySphere(origin, direction, npc.ship.position, npcHitRadius, bestDistance))
            {
                bestDistance = *t;
                best.kind = WeaponHit::Kind::Ship;
                best.ship = i;
                best.point = origin + direction * *t;
                best.normal = normalized(best.point - npc.ship.position);
            }
        }

        if (const auto rock = mining::findLaserTarget(world, origin, direction, bestDistance); rock && rock->distance < bestDistance)
        {
            bestDistance = rock->distance;
            best.kind = WeaponHit::Kind::Rock;
            best.ship = -1;
            best.point = rock->point;
            best.normal = rock->normal;
        }

        if (world.stationActive)
        {
            if (const auto t = rayStation(world.station, origin, direction, bestDistance))
            {
                bestDistance = *t;
                best.kind = WeaponHit::Kind::Station;
                best.ship = -1;
                best.point = origin + direction * *t;
                best.normal = direction * -1.f;
            }
        }

        best.distance = bestDistance;
        return best;
    }

    /** Velocity of whatever was hit, so sparks fly with it. */
    inline Vec3 hitVelocity(const World& world, const WeaponHit& hit)
    {
        if (hit.kind == WeaponHit::Kind::Ship)
            return world.npcShips[static_cast<std::size_t>(hit.ship)].ship.velocity;

        if (hit.kind == WeaponHit::Kind::Station)
            return stationVelocity(world);

        return {};
    }

    /* ---- Damage ------------------------------------------------------------------------------ */

    /** Destroys NPC ship `index`: an explosion and debris, and the ship leaves play until the traffic brings a new one. */
    inline void destroyShip(World& world, int index, std::vector<CombatEvent>& events)
    {
        NpcShip& npc = world.npcShips[static_cast<std::size_t>(index)];
        spawnExplosion(world.combat, npc.ship.position, npc.ship.velocity, 420.f, 42);

        npc.state = NpcState::Inactive;
        npc.stateTimer = 0.f;
        npc.wakeDelay = 60.f + 90.f * nextRandom(world.combat);
        npc.ship.velocity = {};
        npc.fleeing = false;

        const bool wasTarget = world.target.type == TargetType::Ship && world.target.shipIndex == index;

        // A pirate leaves play for good, earns its bounty, and counts towards its bounty mission.
        if (npc.hostile)
        {
            npc.retired = true;
            npc.wakeDelay = 1e9f;
            world.combat.bountyEarned += pirateBounty;

            if (npc.missionId != 0)
                world.combat.missionKills.push_back(npc.missionId);

            events.push_back({CombatEventKind::PirateDestroyed, index});
        }
        else
        {
            events.push_back({wasTarget ? CombatEventKind::TargetDestroyed : CombatEventKind::ShipDestroyed, index});
        }

        if (wasTarget)
            world.target = {};

        std::erase_if(world.combat.flashes, [index](const HitFlash& flash) { return flash.ship == index; });
    }

    /**
     * Deals `amount` damage to NPC ship `index` at world point `point`. Shields take it first (with
     * a flash of the shield bubble); what gets through hits the hull (with sparks). A ship whose
     * shields fail turns tail: full throttle, and it warps out after fleeSeconds unless it is
     * caught first. A ship whose hull reaches zero is destroyed.
     */
    inline void damageShip(World& world, int index, float amount, const Vec3& point, std::vector<CombatEvent>& events)
    {
        if (index < 0 || index >= static_cast<int>(world.npcShips.size()))
            return;

        NpcShip& npc = world.npcShips[static_cast<std::size_t>(index)];

        if (!npc.isVisible() || amount <= 0.f)
            return;

        CombatState& state = world.combat;
        npc.sinceHit = 0.f;

        const bool shieldsWereUp = npc.shields > 0.f;
        const float absorbed = std::min(npc.shields, amount);
        npc.shields -= absorbed;
        npc.hull -= amount - absorbed;

        // A flash where the hit landed: on the shield bubble while it holds, sparks off the hull once it's gone.
        if (absorbed > 0.f)
        {
            if (std::count_if(state.flashes.begin(), state.flashes.end(), [index](const HitFlash& flash) { return flash.ship == index; }) < 6)
                state.flashes.push_back({index, point - npc.ship.position, 0.f, true});
        }
        else
        {
            spawnSparks(state, point, normalized(point - npc.ship.position), npc.ship.velocity, 5, 260.f);
        }

        if (shieldsWereUp && npc.shields <= 0.f)
        {
            events.push_back({CombatEventKind::ShieldsDown, index});

            // It runs for it, unless it is already committed to the station's slot. (Pirates decide for themselves: see piracy.h++.)
            if (!npc.hostile && !npc.fleeing && (npc.state == NpcState::Roaming || npc.state == NpcState::HeadingToStation || npc.state == NpcState::WarpingOut))
            {
                npc.fleeing = true;
                npc.state = NpcState::WarpingOut;
                npc.stateTimer = 1.f - fleeSeconds;
                npc.ship.throttle = 1.f;
                npc.ship.reverseThrust = false;
            }
        }

        if (npc.hull <= 0.f)
            destroyShip(world, index, events);
    }

    /** Splash damage around an explosion: full within a quarter of the radius, falling to nothing at its edge. */
    inline void splashDamage(World& world, const Vec3& centre, float radius, float damage, std::vector<CombatEvent>& events)
    {
        for (int i = 0; i < static_cast<int>(world.npcShips.size()); ++i)
        {
            const NpcShip& npc = world.npcShips[static_cast<std::size_t>(i)];

            if (!npc.isVisible())
                continue;

            const float distance = std::max(0.f, length(npc.ship.position - centre) - npcHitRadius * 0.5f);

            if (distance >= radius)
                continue;

            const float falloff = distance <= radius * 0.25f ? 1.f : 1.f - (distance - radius * 0.25f) / (radius * 0.75f);
            const Vec3 toShip = npc.ship.position - centre;
            const Vec3 point = length(toShip) > 1.f ? npc.ship.position - normalized(toShip) * (npcHitRadius * 0.8f) : centre;
            damageShip(world, i, damage * falloff, point, events);
        }
    }

    /* ---- Firing ------------------------------------------------------------------------------ */

    /** The world position of hardpoint `kind`'s muzzle `index` on the player's ship. */
    inline Vec3 muzzlePosition(const Ship& ship, HardpointKind kind, int index)
    {
        return shipLocalToWorld(ship, hardpointMuzzles(kind)[static_cast<std::size_t>(index & 1)]);
    }

    /** True if `kind` uses the capacitor and there isn't enough left to fire it. */
    inline bool lowOnEnergy(const CombatState& state, HardpointKind kind)
    {
        switch (kind)
        {
            case HardpointKind::PulseLaser: return state.energy < pulseEnergy;
            case HardpointKind::BeamLaser: return state.energy < 1.f;
            case HardpointKind::TurretLaser: return state.energy < turretEnergy;
            default: return false;
        }
    }

    /**
     * Fires (or holds) weapon `kind` for one step. `trigger` is whether it may fire and the pilot is
     * holding the fire key; `justPressed` is true on the first step of a press. `rockets` is the
     * commander's rocket count, spent by the launcher. Sets the readout status either way.
     */
    inline void fireWeapon(World& world, HardpointKind kind, int& rockets, bool trigger, bool justPressed, float dt, std::vector<CombatEvent>& events)
    {
        CombatState& state = world.combat;
        const Ship& ship = world.playerShip;
        const Vec3 forward = shipForward(ship);

        state.status = WeaponStatus::Ready;

        if (lowOnEnergy(state, kind))
            state.status = WeaponStatus::LowEnergy;

        if (kind == HardpointKind::RocketLauncher && rockets <= 0)
            state.status = WeaponStatus::NoAmmo;

        if (kind == HardpointKind::TurretLaser && !state.turretInArc)
            state.status = WeaponStatus::OutOfArc;

        if (!trigger)
            return;

        switch (kind)
        {
            case HardpointKind::PulseLaser:
            {
                if (state.pulseCooldown > 0.f)
                {
                    state.status = WeaponStatus::Firing;
                    break;
                }

                if (state.energy < pulseEnergy)
                    break;

                const Vec3 muzzle = muzzlePosition(ship, kind, state.nextPulseMuzzle);
                state.bolts.push_back({muzzle, ship.velocity + forward * pulseSpeed, pulseRange / pulseSpeed, pulseDamage, false});
                spawnFlash(state, muzzle, ship.velocity, 9.f);
                state.energy -= pulseEnergy;
                state.pulseCooldown = pulseInterval;
                state.nextPulseMuzzle ^= 1;
                state.status = WeaponStatus::Firing;
                break;
            }

            case HardpointKind::BeamLaser:
            {
                if (state.energy < 1.f)
                    break;

                state.beamHeld = true;
                state.status = WeaponStatus::Firing;
                state.beamPower = std::min(1.f, state.beamPower + dt / beamRampSeconds);
                state.energy = std::max(0.f, state.energy - beamEnergyPerSecond * dt);

                const Vec3 origin = (muzzlePosition(ship, kind, 0) + muzzlePosition(ship, kind, 1)) * 0.5f;
                const WeaponHit hit = castWeaponRay(world, origin, forward, beamRange);
                state.beamHitting = hit.kind != WeaponHit::Kind::None;
                state.beamEnd = state.beamHitting ? hit.point : origin + forward * beamRange;

                if (hit.kind == WeaponHit::Kind::Ship)
                    damageShip(world, hit.ship, beamDamagePerSecond * state.beamPower * dt, hit.point, events);

                if (state.beamHitting && nextRandom(state) < dt * 40.f)
                    spawnSparks(state, hit.point, hit.normal, hitVelocity(world, hit), 2, 300.f);

                break;
            }

            case HardpointKind::RocketLauncher:
            {
                if (rockets <= 0)
                {
                    if (justPressed)
                        events.push_back({CombatEventKind::OutOfRockets, -1});

                    break;
                }

                if (state.rocketCooldown > 0.f)
                {
                    state.status = WeaponStatus::Firing;
                    break;
                }

                Rocket rocket;
                rocket.position = muzzlePosition(ship, kind, state.nextRocketPod);
                rocket.velocity = ship.velocity + forward * rocketLaunchSpeed;
                rocket.topSpeed = length(ship.velocity) + rocketSpeed;
                rocket.life = rocketLife;

                // It homes on the locked ship if that ship is roughly ahead and not too far.
                if (const NpcShip* target = targetedShip(world))
                {
                    const Vec3 toTarget = target->ship.position - ship.position;
                    const float distance = length(toTarget);

                    if (distance <= rocketLockRange && distance > 1.f && dot(toTarget / distance, forward) >= rocketLockCone)
                        rocket.targetShip = world.target.shipIndex;
                }

                state.rockets.push_back(rocket);
                spawnFlash(state, rocket.position, ship.velocity, 12.f);
                state.nextRocketPod ^= 1;
                state.rocketCooldown = rocketInterval;
                --rockets;
                state.status = WeaponStatus::Firing;

                if (rockets == 0)
                    events.push_back({CombatEventKind::OutOfRockets, -1});

                break;
            }

            case HardpointKind::TurretLaser:
            {
                if (!state.turretInArc || state.energy < turretEnergy)
                    break;

                state.status = WeaponStatus::Firing;

                if (state.turretCooldown > 0.f || !state.turretOnTarget)
                    break;

                const Vec3 mount = muzzlePosition(ship, kind, 0);
                const Vec3 muzzle = mount + state.turretAim * 40.f;
                state.bolts.push_back({muzzle, ship.velocity + state.turretAim * turretSpeed, turretRange / turretSpeed, turretDamage, true});
                spawnFlash(state, muzzle, ship.velocity, 7.f);
                state.energy -= turretEnergy;
                state.turretCooldown = turretInterval;
                break;
            }

            case HardpointKind::MiningLaser:
                break;
        }
    }

    /**
     * Turns the turret toward where its bolts would meet the locked ship (leading it by its relative
     * velocity), at most turretSlewRate a second. With no ship locked, or the ship below the hull
     * where the turret can't reach, it turns back to face forward.
     */
    inline void aimTurret(World& world, float dt)
    {
        CombatState& state = world.combat;
        const Ship& ship = world.playerShip;
        const Vec3 forward = shipForward(ship);
        const Vec3 mount = muzzlePosition(ship, HardpointKind::TurretLaser, 0);

        if (!state.turretInitialised)
        {
            state.turretAim = forward;
            state.turretInitialised = true;
        }

        Vec3 desired = forward;
        bool hasTarget = false;
        state.turretInArc = true;

        if (const NpcShip* target = targetedShip(world))
        {
            // Lead the target: where it will be when a bolt fired now gets there.
            const Vec3 relativeVelocity = target->ship.velocity - ship.velocity;
            Vec3 aimPoint = target->ship.position;

            for (int iteration = 0; iteration < 3; ++iteration)
                aimPoint = target->ship.position + relativeVelocity * (length(aimPoint - mount) / turretSpeed);

            const Vec3 toAim = aimPoint - mount;

            if (length(toAim) > 1.f)
            {
                const Vec3 direction = normalized(toAim);

                if (dot(direction, shipUp(ship)) >= turretArcFloor && length(toAim) <= turretRange * 1.15f)
                {
                    desired = direction;
                    hasTarget = true;
                }
                else
                {
                    state.turretInArc = false;
                }
            }
        }

        // Turn toward `desired`, no faster than the slew rate.
        const float cosine = std::clamp(dot(state.turretAim, desired), -1.f, 1.f);
        const float angle = std::acos(cosine);
        const float step = turretSlewRate * dt;

        if (angle <= step || angle < 1e-4f)
            state.turretAim = desired;
        else
        {
            Vec3 perpendicular = desired - state.turretAim * cosine;

            if (length(perpendicular) < 1e-5f)
                perpendicular = shipUp(ship);

            perpendicular = normalized(perpendicular);
            state.turretAim = normalized(state.turretAim * std::cos(step) + perpendicular * std::sin(step));
        }

        // With a target it fires only once lined up; with none it fires straight ahead.
        state.turretOnTarget = !hasTarget || dot(state.turretAim, desired) >= turretOnTargetCos;
    }

    /**
     * Deals `amount` damage to the player's ship at world point `point`: shields first (with a
     * flash of the shield bubble), then hull. At zero hull the ship is destroyed: it explodes, and
     * the scene takes over (insurance, see SystemScene).
     */
    inline void damagePlayer(World& world, float amount, const Vec3& point, std::vector<CombatEvent>& events)
    {
        PlayerHealth& player = world.combat.player;

        if (player.destroyed || amount <= 0.f)
            return;

        const Ship& ship = world.playerShip;
        player.sinceHit = 0.f;
        const float absorbed = std::min(player.shields, amount);
        player.shields -= absorbed;
        player.hull -= amount - absorbed;

        if (absorbed > 0.f)
            world.combat.flashes.push_back({playerShip, point - ship.position, 0.f, true});

        if (amount - absorbed > 0.f)
        {
            player.sinceHullHit = 0.f;
            spawnSparks(world.combat, point, normalized(point - ship.position), ship.velocity, 5, 260.f);
        }

        events.push_back({CombatEventKind::PlayerHit, -1});

        if (player.hull <= 0.f)
        {
            player.hull = 0.f;
            player.destroyed = true;
            spawnExplosion(world.combat, ship.position, ship.velocity, 460.f, 46);
            events.push_back({CombatEventKind::PlayerDestroyed, -1});
        }
    }

    /** Moves the bolts one step; a bolt that crosses something on the way hits it. */
    inline void updateBolts(World& world, float dt, std::vector<CombatEvent>& events)
    {
        CombatState& state = world.combat;

        for (Bolt& bolt : state.bolts)
        {
            const Vec3 start = bolt.position;
            bolt.position += bolt.velocity * dt;
            bolt.life -= dt;

            const Vec3 travel = bolt.position - start;
            const float travelled = length(travel);

            if (travelled < 1e-3f)
                continue;

            const Vec3 direction = travel / travelled;

            // A pirate's bolt hits the player (or a rock, or the station), never another ship.
            if (bolt.hostile)
            {
                const WeaponHit scenery = castWeaponRay(world, start, direction, travelled, false);
                const auto playerHit = world.combat.player.destroyed ? std::nullopt : mining::raySphere(start, direction, world.playerShip.position, playerHitRadius, travelled);

                if (playerHit && (scenery.kind == WeaponHit::Kind::None || *playerHit <= scenery.distance))
                {
                    damagePlayer(world, bolt.damage, start + direction * *playerHit, events);
                    bolt.life = -1.f;
                }
                else if (scenery.kind != WeaponHit::Kind::None)
                {
                    spawnSparks(state, scenery.point, scenery.normal, hitVelocity(world, scenery), 4, 240.f);
                    bolt.life = -1.f;
                }

                continue;
            }

            const WeaponHit hit = castWeaponRay(world, start, direction, travelled);

            if (hit.kind == WeaponHit::Kind::None)
                continue;

            if (hit.kind == WeaponHit::Kind::Ship)
                damageShip(world, hit.ship, bolt.damage, hit.point, events);
            else
                spawnSparks(state, hit.point, hit.normal, hitVelocity(world, hit), 4, 240.f);

            bolt.life = -1.f;
        }

        std::erase_if(state.bolts, [](const Bolt& bolt) { return bolt.life <= 0.f; });
    }

    /** Detonates a rocket: an explosion, and splash damage to any ship near it. */
    inline void detonate(World& world, Rocket& rocket, std::vector<CombatEvent>& events)
    {
        spawnExplosion(world.combat, rocket.position, rocket.velocity * 0.15f, rocketSplashRadius, 8);
        splashDamage(world, rocket.position, rocketSplashRadius, rocketDamage, events);
        rocket.life = -1.f;
    }

    /**
     * Flies the rockets one step: they accelerate to their top speed, turn toward where their ship
     * will be (if they have one), trail smoke, and detonate on reaching it, on striking anything
     * else, or when their fuel runs out.
     */
    inline void updateRockets(World& world, float dt, std::vector<CombatEvent>& events)
    {
        CombatState& state = world.combat;

        // New rockets may be added during detonation's events; iterate by index over the ones that exist now.
        const std::size_t count = state.rockets.size();

        for (std::size_t i = 0; i < count; ++i)
        {
            Rocket& rocket = state.rockets[i];

            if (rocket.life <= 0.f)
                continue;

            float speed = length(rocket.velocity);
            Vec3 heading = speed > 1e-3f ? rocket.velocity / speed : Vec3{0.f, 0.f, 1.f};

            // Home on the target ship while it is still there.
            if (rocket.targetShip >= 0 && !shipIsTargetable(world, rocket.targetShip))
                rocket.targetShip = -1;

            if (rocket.targetShip >= 0)
            {
                const NpcShip& target = world.npcShips[static_cast<std::size_t>(rocket.targetShip)];
                const float distance = length(target.ship.position - rocket.position);

                if (distance <= rocketFuse)
                {
                    detonate(world, rocket, events);
                    continue;
                }

                const float timeToGo = distance / std::max(200.f, speed);
                const Vec3 desired = normalized(target.ship.position + target.ship.velocity * timeToGo - rocket.position);
                const float cosine = std::clamp(dot(heading, desired), -1.f, 1.f);
                const float angle = std::acos(cosine);
                const float step = rocketTurnRate * dt;

                if (angle <= step)
                    heading = desired;
                else if (angle > 1e-4f)
                    heading = normalized(heading * std::cos(step) + normalized(desired - heading * cosine) * std::sin(step));
            }

            speed = std::min(rocket.topSpeed, speed + rocketAcceleration * dt);
            rocket.velocity = heading * speed;

            const Vec3 start = rocket.position;
            rocket.position += rocket.velocity * dt;
            rocket.life -= dt;

            // Smoke behind it, drifting slowly.
            rocket.smokeTimer -= dt;

            while (rocket.smokeTimer <= 0.f)
            {
                rocket.smokeTimer += 0.022f;
                Particle puff;
                puff.kind = ParticleKind::Smoke;
                puff.position = rocket.position - heading * 26.f;
                puff.velocity = rocket.velocity * 0.08f + randomDirection(state) * 18.f;
                puff.life = 1.1f + 0.5f * nextRandom(state);
                puff.size = 1.f;
                addParticle(state, puff);
            }

            // Struck something on the way (another ship, a rock, the station)?
            const float travelled = length(rocket.position - start);

            if (travelled > 1e-3f)
            {
                const WeaponHit hit = castWeaponRay(world, start, (rocket.position - start) / travelled, travelled + 30.f);

                if (hit.kind != WeaponHit::Kind::None)
                {
                    rocket.position = hit.point;
                    detonate(world, rocket, events);
                    continue;
                }
            }

            if (rocket.life <= 0.f)
                detonate(world, rocket, events);
        }

        std::erase_if(state.rockets, [](const Rocket& rocket) { return rocket.life <= 0.f; });
    }

    /**
     * One step of everything combat leaves running, whatever is selected: the capacitor recharges,
     * cooldowns tick, the beam fades if it wasn't fired, the turret aims, bolts and rockets fly,
     * particles, explosions and flashes age, and NPC shields recharge. Call after fireWeapon().
     */
    inline void updateCombat(World& world, float dt, std::vector<CombatEvent>& events)
    {
        CombatState& state = world.combat;

        state.energy = std::min(energyCapacity, state.energy + energyRecharge * dt);
        state.pulseCooldown = std::max(0.f, state.pulseCooldown - dt);
        state.rocketCooldown = std::max(0.f, state.rocketCooldown - dt);
        state.turretCooldown = std::max(0.f, state.turretCooldown - dt);

        if (!state.beamHeld)
        {
            state.beamPower = std::max(0.f, state.beamPower - dt / beamRampSeconds);

            if (state.beamPower <= 0.f)
                state.beamHitting = false;
        }

        state.beamHeld = false;

        aimTurret(world, dt);
        updateBolts(world, dt, events);
        updateRockets(world, dt, events);

        for (Particle& particle : state.particles)
        {
            particle.position += particle.velocity * dt;
            particle.age += dt;

            if (particle.kind == ParticleKind::Smoke)
                particle.velocity = particle.velocity * std::exp(-1.5f * dt);
        }

        std::erase_if(state.particles, [](const Particle& particle) { return particle.age >= particle.life; });

        for (Explosion& explosion : state.explosions)
        {
            explosion.position += explosion.velocity * dt;
            explosion.age += dt;
        }

        std::erase_if(state.explosions, [](const Explosion& explosion) { return explosion.age >= explosion.life; });

        for (HitFlash& flash : state.flashes)
            flash.age += dt;

        std::erase_if(state.flashes, [&world](const HitFlash& flash)
        {
            if (flash.age >= 0.35f)
                return true;

            if (flash.ship == playerShip)
                return world.combat.player.destroyed;

            return flash.ship < 0 || flash.ship >= static_cast<int>(world.npcShips.size()) || !world.npcShips[static_cast<std::size_t>(flash.ship)].isVisible();
        });

        // The player's shields come back after a pause without being hit.
        PlayerHealth& player = state.player;
        player.sinceHit += dt;
        player.sinceHullHit += dt;

        if (!player.destroyed && player.sinceHit >= playerShieldDelay && player.shields < playerMaxShields)
            player.shields = std::min(playerMaxShields, player.shields + playerShieldRate * dt);

        // Shields come back after a pause without being hit.
        for (NpcShip& npc : world.npcShips)
        {
            npc.sinceHit += dt;

            if (npc.hull > 0.f && npc.sinceHit >= shieldRechargeDelay && npc.shields < npcMaxShields)
                npc.shields = std::min(npcMaxShields, npc.shields + shieldRechargeRate * dt);
        }
    }

    /** The weapon status as a short label for the HUD. */
    inline const char* weaponStatusLabel(WeaponStatus status)
    {
        switch (status)
        {
            case WeaponStatus::Ready: return "READY";
            case WeaponStatus::Firing: return "FIRING";
            case WeaponStatus::LowEnergy: return "LOW ENERGY";
            case WeaponStatus::NoAmmo: return "NO ROCKETS";
            case WeaponStatus::OutOfArc: return "TARGET OUT OF ARC";
        }

        return "";
    }

} // namespace combat

#endif //DUSK_COMBAT_H
