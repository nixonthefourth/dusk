//
// Piracy: hostile ships that hunt the player.
//
// Pirates turn up in poor systems (sometimes in developing ones, never in progressive ones) and as
// the targets of bounty missions. They are NPC ships in the Hostile state, flown here rather than
// by the ordinary NPC AI: they close on the player, make firing runs with twin pulse lasers when
// the player is in their sights, keep clear of station space (where the station would protect the
// player), and run for it when badly damaged, unless they are bounty targets, which fight to the end.
//
// A pirate that is destroyed, escapes or is left far behind is retired: it leaves play, and its
// slot in World::npcShips is reused for the next pirate, so the system's traffic doesn't grow.
//

#ifndef DUSK_PIRACY_H
#define DUSK_PIRACY_H

#include "objects/mission.h++"
#include "objects/npc_ship.h++"
#include "procgen/statistical.h++"
#include "systems/combat.h++"
#include "systems/ship_physics.h++"
#include "world/world.h++"
#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace piracy {

    /* ---- Tuning ------------------------------------------------------------------------------ */

    /** A pirate's guns: reach, seconds between shots, damage per bolt, bolt speed, and how closely it must be aimed. */
    constexpr float gunRange = 2200.f;
    constexpr float gunInterval = 0.32f;
    constexpr float gunDamage = 3.5f;
    constexpr float boltSpeed = 3000.f;
    constexpr float aimCone = 0.9976f; // about 4 degrees

    /** The distance a pirate tries to fight at, and the throttle it chases with. */
    constexpr float fightDistance = 1100.f;
    constexpr float chaseThrottle = 0.5f;

    /** Closer than this fraction of the fighting distance, a pirate peels away to set up another pass. */
    constexpr float peelFraction = 0.65f;

    /** Pirates won't fight within this distance of a station: they back off until the player leaves. */
    constexpr float stationSafeRadius = 8000.f;

    /** A pirate runs once its hull falls below this fraction, and is gone after running this long. */
    constexpr float fleeHullFraction = 0.35f;
    constexpr float fleeSeconds = 4.f;

    /** A pirate this far from the player gives up and leaves. */
    constexpr float despawnDistance = 45000.f;

    /** Pirates appear this far from the player. */
    constexpr float spawnDistanceMin = 6500.f;
    constexpr float spawnDistanceMax = 9000.f;

    /** Seconds after entering a system (or after a respawn) before pirates may appear. */
    constexpr float arrivalGrace = 40.f;

    /** Raiders hold this far from the station's centre while firing on it, and turn on the player within this distance. */
    constexpr float raidStandoff = 2400.f;
    constexpr float raiderEngagesPlayer = 1800.f;

    /** Raiders bombard the station at this fraction of their dogfighting rate (it gives a defender time to arrive). */
    constexpr float stationFireSlowdown = 2.f;

    /** A raider that has been hit turns on the player for this long. */
    constexpr float raiderGrudgeSeconds = 6.f;

    /** A raid starts when the player is within this distance of the station it is aimed at. */
    constexpr float raidTriggerDistance = 30000.f;

    /** An outpost: its hull points, how far from the station it is built, and its turrets' reach, rate and damage. */
    constexpr float outpostMaxHull = 450.f;
    constexpr float outpostDistance = 45000.f;
    constexpr float turretRange = 2600.f;
    constexpr float turretInterval = 0.45f;
    constexpr float turretDamage = 4.f;
    constexpr float turretBoltSpeed = 2800.f;

    /** The outpost's fighters launch when the player comes within this distance. */
    constexpr float defenderTrigger = 12000.f;

    /* ---- Spawning and retiring --------------------------------------------------------------- */

    /** Takes pirate `npc` out of play for good: its slot is free for the next pirate. */
    inline void retire(World& world, int index)
    {
        NpcShip& npc = world.npcShips[static_cast<std::size_t>(index)];
        npc.state = NpcState::Inactive;
        npc.retired = true;
        npc.wakeDelay = 1e9f;
        npc.ship.velocity = {};

        if (world.target.type == TargetType::Ship && world.target.shipIndex == index)
            world.target = {};
    }

    /** True for a pirate that is in play. */
    inline bool activePirate(const NpcShip& npc)
    {
        return npc.hostile && !npc.retired && npc.state == NpcState::Hostile;
    }

    /** How many pirates are in play (optionally only those of one bounty mission). */
    inline int activePirates(const World& world, std::uint64_t missionId = 0)
    {
        return static_cast<int>(std::count_if(world.npcShips.begin(), world.npcShips.end(), [missionId](const NpcShip& npc)
        {
            return activePirate(npc) && (missionId == 0 || npc.missionId == missionId);
        }));
    }

    /**
     * Brings a pirate into play at `position`, facing the player: a retired pirate's slot if there
     * is one, otherwise a new slot. Returns its index.
     */
    inline int spawnPirate(World& world, const Vec3& position, bool fearless, std::uint64_t missionId, std::mt19937& rng, bool raider = false)
    {
        int index = -1;

        for (int i = 0; i < static_cast<int>(world.npcShips.size()); ++i)
        {
            const NpcShip& npc = world.npcShips[static_cast<std::size_t>(i)];

            if (npc.retired && npc.state == NpcState::Inactive)
            {
                index = i;
                break;
            }
        }

        if (index < 0)
        {
            NpcShip fresh;

            // Same hull as everyone else: borrow a model from an existing ship.
            fresh.ship = world.npcShips.empty() ? world.playerShip : world.npcShips.front().ship;
            world.npcShips.push_back(fresh);
            index = static_cast<int>(world.npcShips.size()) - 1;
        }

        NpcShip& npc = world.npcShips[static_cast<std::size_t>(index)];
        const Vec3 toPlayer = world.playerShip.position - position;

        npc.state = NpcState::Hostile;
        npc.stateTimer = 0.f;
        npc.hostile = true;
        npc.retired = false;
        npc.fearless = fearless;
        npc.missionId = missionId;
        npc.raider = raider;
        npc.shields = npcMaxShields;
        npc.hull = npcMaxHull;
        npc.sinceHit = 1e9f;
        npc.fleeing = false;
        npc.fleeTimer = 0.f;
        npc.fireCooldown = 1.5f + std::uniform_real_distribution<float>(0.f, 1.5f)(rng);
        npc.nextGun = 0;
        npc.circleSign = std::uniform_real_distribution<float>(0.f, 1.f)(rng) < 0.5f ? -1.f : 1.f;

        npc.ship.position = position;
        npc.ship.previousPosition = position;
        npc.ship.velocity = world.playerShip.velocity;
        npc.ship.yaw = std::atan2(toPlayer.x, toPlayer.z);
        npc.ship.pitch = std::asin(std::clamp(normalized(toPlayer).y, -1.f, 1.f));
        npc.ship.roll = 0.f;
        npc.ship.throttle = chaseThrottle;
        npc.ship.reverseThrust = false;
        npc.ship.flightAssist = true;
        npc.ship.cruiseEngaged = false;
        resetShipRotationState(npc.ship);
        return index;
    }

    /** A point `spawnDistanceMin`-`spawnDistanceMax` from the player, roughly level with it, for pirates to appear at. */
    inline Vec3 spawnPoint(const World& world, std::mt19937& rng)
    {
        std::uniform_real_distribution<float> unit(0.f, 1.f);
        const float angle = unit(rng) * 6.2832f;
        const float distance = spawnDistanceMin + (spawnDistanceMax - spawnDistanceMin) * unit(rng);
        const Vec3 direction = normalized(Vec3{std::cos(angle), (unit(rng) - 0.5f) * 0.4f, std::sin(angle)});
        return world.playerShip.position + direction * distance;
    }

    /* ---- Flying ------------------------------------------------------------------------------ */

    /** Turns `ship` toward `direction` with the same yaw and pitch controls a pilot has. */
    inline void steerToward(Ship& ship, const Vec3& direction)
    {
        const float desiredYaw = std::atan2(direction.x, direction.z);
        const float desiredPitch = std::asin(std::clamp(direction.y, -1.f, 1.f));
        ship.yawInput = std::clamp(wrapAngle(desiredYaw - ship.yaw) * 2.5f, -1.f, 1.f);
        ship.pitchInput = std::clamp((desiredPitch - ship.pitch) * 2.5f, -1.f, 1.f);
        ship.rollInput = 0.f;
        ship.precisionInput = false;
    }

    /**
     * One step for every pirate. A fighting pirate leads the player with its nose, chases to its
     * fighting distance (overshooting into firing passes as it goes) and fires whenever the player
     * is in its sights and in range. Near a station it breaks off and holds fire. Badly damaged, it
     * runs, and is gone after fleeSeconds. Left far behind, it leaves.
     */
    inline void updatePirates(World& world, float dt, std::vector<combat::CombatEvent>& events)
    {
        const Ship& player = world.playerShip;
        const bool playerDown = world.combat.player.destroyed;
        const bool playerNearStation = world.stationActive && length(player.position - world.station.position) < stationSafeRadius;

        for (int index = 0; index < static_cast<int>(world.npcShips.size()); ++index)
        {
            NpcShip& npc = world.npcShips[static_cast<std::size_t>(index)];

            if (!activePirate(npc))
                continue;

            Ship& ship = npc.ship;
            const Vec3 toPlayer = player.position - ship.position;
            const float distance = length(toPlayer);
            npc.stateTimer += dt;
            npc.fireCooldown = std::max(0.f, npc.fireCooldown - dt);

            if (distance > despawnDistance)
            {
                retire(world, index);
                continue;
            }

            // Badly hurt (and not a bounty target fighting to the end): run, then go.
            if (!npc.fearless && !npc.fleeing && npc.hull < npcMaxHull * fleeHullFraction)
                npc.fleeing = true;

            if (npc.fleeing)
            {
                npc.fleeTimer += dt;
                steerToward(ship, distance > 1.f ? toPlayer * (-1.f / distance) : shipForward(ship));
                ship.throttle = 1.f;
                integrateShipPhysics(ship, dt);

                if (npc.fleeTimer >= fleeSeconds)
                {
                    retire(world, index);
                    events.push_back({combat::CombatEventKind::PirateFled, index});
                }

                continue;
            }

            // A raider goes for the station, unless the player is close enough to deal with first, or
            // has been shooting at it in the last few seconds.
            if (npc.raider && world.stationActive && (playerDown || (distance > raiderEngagesPlayer && npc.sinceHit > raiderGrudgeSeconds)))
            {
                const Vec3 toStation = world.station.position - ship.position;
                const float stationDistance = length(toStation);
                const Vec3 aimAtStation = stationDistance > 1.f ? toStation / stationDistance : shipForward(ship);

                if (stationDistance < raidStandoff * 0.6f)
                {
                    Vec3 side = cross(aimAtStation, Vec3{0.f, 1.f, 0.f});
                    side = length(side) > 1e-3f ? normalized(side) : Vec3{1.f, 0.f, 0.f};
                    steerToward(ship, normalized(side * npc.circleSign + aimAtStation * -0.4f));
                    ship.throttle = 0.5f;
                }
                else
                {
                    steerToward(ship, aimAtStation);
                    ship.throttle = stationDistance > raidStandoff * 1.6f ? chaseThrottle : 0.2f;
                }

                integrateShipPhysics(ship, dt);

                if (npc.fireCooldown <= 0.f && stationDistance <= raidStandoff + 900.f && dot(shipForward(ship), aimAtStation) >= aimCone)
                {
                    const auto muzzles = hardpointMuzzles(HardpointKind::PulseLaser);
                    const Vec3 muzzle = shipLocalToWorld(ship, muzzles[static_cast<std::size_t>(npc.nextGun & 1)]);
                    world.combat.bolts.push_back({muzzle, ship.velocity + shipForward(ship) * boltSpeed, (raidStandoff + 1500.f) / boltSpeed, gunDamage, false, true});
                    combat::spawnFlash(world.combat, muzzle, ship.velocity, 9.f);
                    npc.nextGun ^= 1;
                    npc.fireCooldown = gunInterval * stationFireSlowdown * (0.85f + 0.3f * combat::nextRandom(world.combat));
                }

                continue;
            }

            if (playerDown || (playerNearStation && !npc.raider))
            {
                // The station would protect the player (or there is nothing left to shoot): back off and wait.
                const Vec3 away = world.stationActive ? ship.position - world.station.position : ship.position - player.position;
                steerToward(ship, length(away) > 1.f ? normalized(away) : shipForward(ship));
                ship.throttle = length(ship.position - world.station.position) < stationSafeRadius + 3000.f ? 0.5f : 0.f;
                integrateShipPhysics(ship, dt);
                continue;
            }

            // Lead the player: aim where a bolt fired now would meet them.
            const Vec3 relativeVelocity = player.velocity - ship.velocity;
            Vec3 aimPoint = player.position;

            for (int iteration = 0; iteration < 2; ++iteration)
                aimPoint = player.position + relativeVelocity * (length(aimPoint - ship.position) / boltSpeed);

            Vec3 aim = aimPoint - ship.position;
            aim = length(aim) > 1.f ? normalized(aim) : shipForward(ship);

            // Too close: peel away to one side to set up another pass, rather than sitting on top of the player.
            if (distance < fightDistance * peelFraction)
            {
                Vec3 side = cross(aim, Vec3{0.f, 1.f, 0.f});
                side = length(side) > 1e-3f ? normalized(side) : Vec3{1.f, 0.f, 0.f};
                steerToward(ship, normalized(side * npc.circleSign + aim * -0.3f));
                ship.throttle = 0.6f;
            }
            else
            {
                steerToward(ship, aim);
                ship.throttle = distance > fightDistance * 1.6f ? chaseThrottle : (distance > fightDistance ? 0.3f : 0.18f);
            }

            integrateShipPhysics(ship, dt);

            // Fire whenever the player is in its sights and in range.
            if (npc.fireCooldown <= 0.f && distance <= gunRange && dot(shipForward(ship), aim) >= aimCone)
            {
                const auto muzzles = hardpointMuzzles(HardpointKind::PulseLaser);
                const Vec3 muzzle = shipLocalToWorld(ship, muzzles[static_cast<std::size_t>(npc.nextGun & 1)]);
                world.combat.bolts.push_back({muzzle, ship.velocity + shipForward(ship) * boltSpeed, gunRange / boltSpeed, gunDamage, false, true});
                combat::spawnFlash(world.combat, muzzle, ship.velocity, 9.f);
                npc.nextGun ^= 1;
                npc.fireCooldown = gunInterval * (0.85f + 0.3f * combat::nextRandom(world.combat));
            }
        }
    }

    /* ---- Deciding when pirates turn up ------------------------------------------------------- */

    /** How often pirates try their luck in a system of this economy: chance per roll, and the seconds between rolls. */
    struct PirateOdds {
        float chance = 0.f;
        float minimumWait = 1e9f;
        float maximumWait = 1e9f;
        int largestGang = 0;
    };

    /** Pirates prey on poor systems, sometimes developing ones, and never progressive ones. */
    inline PirateOdds oddsFor(EconomyTier tier)
    {
        switch (tier)
        {
            case EconomyTier::Poor: return {0.6f, 110.f, 280.f, 3};
            case EconomyTier::Developing: return {0.25f, 220.f, 520.f, 2};
            case EconomyTier::Progressive: return {0.f, 1e9f, 1e9f, 0};
        }

        return {};
    }

    /** Keeps track of when pirates may next turn up in the current system. */
    struct PirateDirector {
        float grace = arrivalGrace;
        float timer = 90.f;
        float bountyDelay = 8.f;
        std::mt19937 rng{0xB10DCAFEu};
    };

    /** Starts the director afresh, as on entering a system or after a respawn. */
    inline void resetDirector(PirateDirector& director, std::uint32_t seed, EconomyTier tier)
    {
        director.rng.seed(seed);
        director.grace = arrivalGrace;
        const PirateOdds odds = oddsFor(tier);
        director.timer = odds.chance > 0.f ? std::uniform_real_distribution<float>(odds.minimumWait, odds.maximumWait)(director.rng) : 1e9f;
        director.bountyDelay = 8.f;
    }

    /**
     * One step of deciding whether pirates turn up. `canSpawn` is false while the player is docked,
     * docking, in cruise, in hyperspace, destroyed, or close to a station. `bounties` are the
     * commander's bounty missions whose targets are in this system and still flying: their gangs
     * appear (soon after the player is out in open space) whenever none of them is in play. Other
     * pirates come at random intervals, as often as the system's economy invites.
     */
    inline void directPirates(PirateDirector& director, World& world, EconomyTier tier, const std::vector<const Mission*>& bounties, bool canSpawn, float dt, std::vector<combat::CombatEvent>& events)
    {
        if (!canSpawn)
        {
            director.bountyDelay = std::max(director.bountyDelay, 6.f);
            return;
        }

        director.grace = std::max(0.f, director.grace - dt);
        director.bountyDelay = std::max(0.f, director.bountyDelay - dt);

        // Bounty targets: they are waiting here for the player.
        if (director.bountyDelay <= 0.f)
        {
            for (const Mission* mission : bounties)
            {
                if (mission->piratesLeft <= 0 || activePirates(world, mission->id) > 0)
                    continue;

                const Vec3 centre = spawnPoint(world, director.rng);

                for (int i = 0; i < mission->piratesLeft; ++i)
                {
                    const Vec3 offset{static_cast<float>(i) * 350.f - 350.f, static_cast<float>(i % 2) * 200.f, 0.f};
                    spawnPirate(world, centre + offset, true, mission->id, director.rng);
                }

                events.push_back({combat::CombatEventKind::BountyTargetsSighted, -1});
            }
        }

        // Ordinary pirates, if none are about already.
        if (director.grace > 0.f || activePirates(world) > 0)
            return;

        director.timer -= dt;

        if (director.timer > 0.f)
            return;

        const PirateOdds odds = oddsFor(tier);
        director.timer = std::uniform_real_distribution<float>(odds.minimumWait, odds.maximumWait)(director.rng);

        if (std::uniform_real_distribution<float>(0.f, 1.f)(director.rng) >= odds.chance || odds.largestGang <= 0)
            return;

        const int count = std::uniform_int_distribution<int>(1, odds.largestGang)(director.rng);
        const Vec3 centre = spawnPoint(world, director.rng);

        for (int i = 0; i < count; ++i)
        {
            const Vec3 offset{static_cast<float>(i) * 400.f - 400.f, static_cast<float>(i % 2) * 250.f, static_cast<float>(i) * 150.f};
            spawnPirate(world, centre + offset, false, 0, director.rng);
        }

        events.push_back({combat::CombatEventKind::PiratesArrived, count});
    }

    /* ---- Raids and outposts ------------------------------------------------------------------- */

    /** Launches a raid of `count` raiders on the station, from about 9 km out. */
    inline void spawnRaid(World& world, std::uint64_t missionId, int count, std::mt19937& rng)
    {
        std::uniform_real_distribution<float> unit(0.f, 1.f);
        const float angle = unit(rng) * 6.2832f;
        const Vec3 direction = normalized(Vec3{std::cos(angle), (unit(rng) - 0.5f) * 0.3f, std::sin(angle)});
        const Vec3 centre = world.station.position + direction * 9000.f;

        for (int i = 0; i < count; ++i)
        {
            const Vec3 offset{static_cast<float>(i % 3) * 450.f - 450.f, static_cast<float>(i / 3) * 400.f, static_cast<float>(i) * 120.f};
            spawnPirate(world, centre + offset, true, missionId, rng, true);
        }
    }

    /**
     * Builds the outpost for strike mission `missionId`: the station's hull at half size, out in
     * open space outpostDistance from the station (further, if that would be inside a planet), in a
     * direction that depends only on the mission, so it is in the same place every visit.
     */
    inline void placeOutpost(World& world, std::uint64_t missionId)
    {
        Outpost& outpost = world.outpost;
        outpost.station = world.station;
        outpost.station.dockingPort.valid = false;
        outpost.station.rotationSpeed = 0.f;

        for (Vec3& vertex : outpost.station.model.vertices)
            vertex = vertex * 0.5f;

        const float angle = static_cast<float>(missionId % 3600u) * 0.1f * 0.0174533f;
        const float tilt = (static_cast<float>((missionId / 3600u) % 100u) / 100.f - 0.5f) * 0.4f;
        const Vec3 direction = normalized(Vec3{std::cos(angle), tilt, std::sin(angle)});
        float distance = outpostDistance;

        const auto insideAPlanet = [&](const Vec3& position)
        {
            for (const Planet& planet : world.planets)
            {
                if (length(position - planet.position) < planet.radius * 1.6f)
                    return true;
            }

            return world.star.isStar && length(position - world.star.position) < world.star.radius * 2.f;
        };

        while (insideAPlanet(world.station.position + direction * distance) && distance < outpostDistance * 6.f)
            distance += 10000.f;

        outpost.station.position = world.station.position + direction * distance;
        outpost.hull = outpostMaxHull;
        outpost.missionId = missionId;
        outpost.turretCooldown = 2.f;
        outpost.nextTurret = 0;
        outpost.defendersLaunched = false;
        outpost.active = true;
    }

    /** Where the outpost's four turrets sit, in world space: on top, below and to either side, just clear of the hull. */
    inline std::array<Vec3, 4> outpostTurrets(const Outpost& outpost)
    {
        const float reach = combat::outpostRadius(outpost) * 0.62f;
        const Vec3 centre = outpost.station.position;
        return {centre + outpost.station.axisY * reach, centre - outpost.station.axisY * reach,
                centre + outpost.station.axisX * reach, centre - outpost.station.axisX * reach};
    }

    /**
     * One step of the outpost: its turrets fire at the player (leading them) whenever they are in
     * reach, taking turns, and when the player first comes near, two fighters launch to defend it.
     */
    inline void updateOutpost(World& world, float dt, std::vector<combat::CombatEvent>& events, std::mt19937& rng)
    {
        Outpost& outpost = world.outpost;

        if (!outpost.active || world.combat.player.destroyed)
            return;

        const Ship& player = world.playerShip;
        const float distance = length(player.position - outpost.station.position);

        if (!outpost.defendersLaunched && distance < defenderTrigger)
        {
            outpost.defendersLaunched = true;

            for (int i = 0; i < 2; ++i)
            {
                const Vec3 offset{static_cast<float>(i) * 600.f - 300.f, 500.f, 0.f};
                spawnPirate(world, outpost.station.position + offset + normalized(player.position - outpost.station.position) * (combat::outpostRadius(outpost) + 600.f), true, 0, rng);
            }

            events.push_back({combat::CombatEventKind::OutpostSighted, -1});
        }

        outpost.turretCooldown = std::max(0.f, outpost.turretCooldown - dt);

        if (outpost.turretCooldown > 0.f)
            return;

        const auto turrets = outpostTurrets(outpost);
        const Vec3 turret = turrets[static_cast<std::size_t>(outpost.nextTurret % 4)];
        outpost.nextTurret = (outpost.nextTurret + 1) % 4;
        const float reach = length(player.position - turret);

        if (reach > turretRange)
            return;

        // Lead the player, as a pirate does.
        Vec3 aimPoint = player.position;

        for (int iteration = 0; iteration < 2; ++iteration)
            aimPoint = player.position + player.velocity * (length(aimPoint - turret) / turretBoltSpeed);

        const Vec3 aim = normalized(aimPoint - turret);
        world.combat.bolts.push_back({turret, aim * turretBoltSpeed, turretRange / turretBoltSpeed, turretDamage, false, true});
        combat::spawnFlash(world.combat, turret, Vec3{}, 11.f);
        outpost.turretCooldown = turretInterval;
    }

    /**
     * Starts what defence and strike missions in this system need. A defence mission's raid comes
     * (its raiders still flying) whenever the player is out near the station and none of its
     * raiders are in play. A strike mission's outpost is built if it isn't there, and taken away
     * when no strike mission here wants it any more.
     */
    inline void directAssaults(World& world, const std::vector<const Mission*>& defences, const std::vector<const Mission*>& strikes, bool playerOut, std::vector<combat::CombatEvent>& events, std::mt19937& rng)
    {
        const bool nearStation = world.stationActive && length(world.playerShip.position - world.station.position) < raidTriggerDistance;

        for (const Mission* mission : defences)
        {
            if (mission->piratesLeft <= 0 || !playerOut || !nearStation || activePirates(world, mission->id) > 0)
                continue;

            spawnRaid(world, mission->id, mission->piratesLeft, rng);
            events.push_back({combat::CombatEventKind::RaidStarted, mission->piratesLeft});
        }

        const Mission* wanted = nullptr;

        for (const Mission* mission : strikes)
        {
            if (mission->piratesLeft > 0)
            {
                wanted = mission;
                break;
            }
        }

        if (!wanted)
        {
            world.outpost.active = false;
            return;
        }

        if (!world.outpost.active || world.outpost.missionId != wanted->id)
            placeOutpost(world, wanted->id);
    }

    /** True while any raider is in play: the station's no-fire zone is lifted so the player can fight them. */
    inline bool raidUnderway(const World& world)
    {
        return std::any_of(world.npcShips.begin(), world.npcShips.end(), [](const NpcShip& npc) { return activePirate(npc) && npc.raider; });
    }

    /** Retires every pirate in play (after the player's ship is lost, for instance). */
    inline void retireAll(World& world)
    {
        for (int i = 0; i < static_cast<int>(world.npcShips.size()); ++i)
        {
            if (activePirate(world.npcShips[static_cast<std::size_t>(i)]))
                retire(world, i);
        }
    }

} // namespace piracy

#endif //DUSK_PIRACY_H
