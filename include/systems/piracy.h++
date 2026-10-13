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
    inline int spawnPirate(World& world, const Vec3& position, bool fearless, std::uint64_t missionId, std::mt19937& rng)
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

            if (playerDown || playerNearStation)
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
