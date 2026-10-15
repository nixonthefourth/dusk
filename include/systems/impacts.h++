//
// Impacts: what hitting things does to the player's ship, and what air does to it at speed.
//
//   - Collisions hurt. A knock harder than harmlessSpeed damages the ship (shields first, as
//     for weapons fire), and an off-centre one sets it tumbling for a moment. Rocks, the station's
//     hull and other ships all count.
//   - Ships collide with each other as rigid bodies: the player and an NPC ship bounce apart,
//     sharing the impulse by their masses, and both take damage.
//   - Planets are solid and fatal: touching a surface faster than planetCrashSpeed destroys the
//     ship. The star destroys it on contact.
//   - Every planet has an atmosphere. Inside it the air drags the ship back toward the planet's
//     own motion and heats the hull in proportion to speed and density; past burnThreshold the
//     hull burns. The star has a corona that heats a ship however slowly it moves.
//
// Plain logic over the World, with no drawing, so it can be tested headless. The scene runs it
// after the world step, while the player is flying (not while the docking computer is).
//

#ifndef DUSK_IMPACTS_H
#define DUSK_IMPACTS_H

#include "objects/npc_ship.h++"
#include "objects/planet.h++"
#include "objects/ship.h++"
#include "systems/combat.h++"
#include "systems/ship_physics.h++"
#include "world/world.h++"
#include <algorithm>
#include <cmath>
#include <vector>

namespace impacts {

    /* ---- Tuning ------------------------------------------------------------------------------ */

    /** Knocks slower than this do no damage; above it, each extra unit per second costs damagePerSpeed points. */
    constexpr float harmlessSpeed = 25.f;
    constexpr float damagePerSpeed = 0.35f;

    /** Touching a planet's surface faster than this destroys the ship. */
    constexpr float planetCrashSpeed = 40.f;

    /** A planet's atmosphere reaches this fraction of its radius above the surface; a star's corona, this much. */
    constexpr float atmosphereDepth = 0.15f;
    constexpr float coronaDepth = 0.6f;

    /** Air drag at the bottom of an atmosphere: the share of speed (relative to the planet) lost per second. */
    constexpr float dragRate = 0.7f;

    /** Heating: none below safeEntrySpeed; above it, heatPerSpeed degrees a second per unit per second at full density. */
    constexpr float safeEntrySpeed = 150.f;
    constexpr float heatPerSpeed = 0.12f;

    /** Scales a planet's friction heating (stars have their own, coronaHeating). */
    constexpr float frictionScale = 4.f;

    /** A star's corona heats this fast at its base, however slowly the ship moves. */
    constexpr float coronaHeating = 260.f;

    /** Out of the air the hull cools this fast; past burnThreshold it burns at burnDamage hull points a second per degree over. */
    constexpr float coolingRate = 30.f;
    constexpr float burnThreshold = 100.f;
    constexpr float burnDamage = 0.6f;
    constexpr float maximumHeat = 200.f;

    /** Ships as rigid spheres for colliding with each other: their radius, and how bouncy the hit is. */
    constexpr float shipRadius = 180.f;
    constexpr float shipRestitution = 0.35f;

    /* ---- Damage and tumbling ----------------------------------------------------------------- */

    /** Hull and shield damage from a knock at `speed` units per second. */
    inline float impactDamage(float speed)
    {
        return speed <= harmlessSpeed ? 0.f : (speed - harmlessSpeed) * damagePerSpeed;
    }

    /**
     * Sets `ship` tumbling from a knock at `point` along `normal` (pointing away from what was
     * hit) at `speed`: the spin is about the axis the knock would turn it on, harder the harder
     * and the further off-centre it lands, and lasts a moment longer the harder it is.
     */
    inline void tumble(Ship& ship, const Vec3& point, const Vec3& normal, float speed)
    {
        if (speed <= harmlessSpeed)
            return;

        const Vec3 lever = point - ship.position;
        Vec3 axis = cross(lever, normal);

        if (length(axis) < 1e-3f)
            axis = cross(shipForward(ship), normal);

        if (length(axis) < 1e-3f)
            return;

        axis = normalized(axis);
        const float spin = std::min(2.5f, speed / 120.f);
        ship.tumbleYaw = ship.yawRate + dot(axis, shipUp(ship)) * spin;
        ship.tumblePitch = ship.pitchRate + dot(axis, shipRight(ship)) * spin;
        ship.tumbleRoll = ship.rollRate + dot(axis, shipForward(ship)) * spin;
        ship.tumbleTimer = std::clamp(speed / 100.f, 0.5f, 2.5f);
    }

    /* ---- Atmospheres ------------------------------------------------------------------------- */

    /** How thick `body`'s air is at `position`: 0 outside it, rising to 1 at the surface (as the square of the depth into it). */
    inline float airDensity(const Planet& body, const Vec3& position)
    {
        const float depth = body.radius * (body.isStar ? coronaDepth : atmosphereDepth);

        if (depth <= 0.f)
            return 0.f;

        const float altitude = length(position - body.position) - body.radius;

        if (altitude >= depth)
            return 0.f;

        const float into = 1.f - std::max(0.f, altitude) / depth;
        return into * into;
    }

    /** The planet whose air the ship is in (the thickest), or nullptr; and its density there. */
    inline const Planet* airAround(const World& world, const Vec3& position, float& density)
    {
        const Planet* thickest = nullptr;
        density = 0.f;

        const auto consider = [&](const Planet& body)
        {
            const float here = airDensity(body, position);

            if (here > density)
            {
                density = here;
                thickest = &body;
            }
        };

        if (world.star.isStar)
            consider(world.star);

        for (const Planet& planet : world.planets)
            consider(planet);

        return thickest;
    }

    /**
     * One step of air: drag pulls the ship's velocity toward the planet's, and friction heats the
     * hull (a star's corona heats it regardless of speed). Out of the air it cools. Over the burn
     * threshold the hull takes damage, and the hot hull throws off plasma. Cruise can't be held
     * in an atmosphere. Destroying the ship this way is "burned up".
     */
    inline void updateAir(World& world, float dt, std::vector<combat::CombatEvent>& events)
    {
        Ship& ship = world.playerShip;
        float density = 0.f;
        const Planet* body = airAround(world, ship.position, density);

        if (body)
        {
            const Vec3 relative = ship.velocity - body->velocity;
            const float speed = length(relative);

            if (!body->isStar)
            {
                ship.velocity = body->velocity + relative * std::exp(-dragRate * density * dt);

                if (ship.cruiseEngaged)
                    disengageCruise(ship);
            }

            // Friction heating against steady cooling: heat builds only when friction outpaces it.
            const float heating = body->isStar
                ? coronaHeating * std::sqrt(density) /* = how far into the corona */ + std::max(0.f, speed - safeEntrySpeed) * heatPerSpeed * density
                : std::max(0.f, speed - safeEntrySpeed) * heatPerSpeed * density * frictionScale;

            ship.heat = std::clamp(ship.heat + (heating - coolingRate) * dt, 0.f, maximumHeat);
        }
        else
        {
            ship.heat = std::max(0.f, ship.heat - coolingRate * dt);
        }

        if (ship.heat <= 0.f)
            return;

        // Glowing: plasma streams off the leading face, more of it the hotter the hull.
        const float speed = length(ship.velocity - (body ? body->velocity : Vec3{}));
        const Vec3 heading = speed > 1.f ? (ship.velocity - (body ? body->velocity : Vec3{})) / speed : shipForward(ship);

        if (combat::nextRandom(world.combat) < std::min(1.f, ship.heat / 60.f))
            combat::spawnSparks(world.combat, ship.position + heading * 160.f, heading * -1.f, ship.velocity, 1 + static_cast<int>(ship.heat / 50.f), 260.f + ship.heat * 2.f);

        if (ship.heat > burnThreshold)
            combat::damagePlayer(world, (ship.heat - burnThreshold) * burnDamage * dt, ship.position + heading * 150.f, events, body && body->isStar ? "BURNED UP IN THE STAR'S CORONA" : "BURNED UP IN THE ATMOSPHERE");
    }

    /* ---- Collisions -------------------------------------------------------------------------- */

    /**
     * What the contact resolvers recorded this step: a planet hit too fast, or the star, destroys
     * the ship; rocks and gentler knocks damage it (shields first) and set it tumbling.
     */
    inline void applyContacts(World& world, std::vector<combat::CombatEvent>& events)
    {
        Ship& ship = world.playerShip;

        for (const ContactRecord& contact : world.playerContacts)
        {
            if (contact.kind == ContactRecord::Kind::Star)
            {
                combat::destroyPlayer(world, "FLEW INTO THE STAR", events);
                continue;
            }

            if (contact.kind == ContactRecord::Kind::Planet && contact.speed > planetCrashSpeed)
            {
                combat::destroyPlayer(world, "CRASHED INTO A PLANET", events);
                continue;
            }

            const float damage = impactDamage(contact.speed);

            if (damage > 0.f)
            {
                combat::damagePlayer(world, damage, contact.point, events, contact.kind == ContactRecord::Kind::Rock ? "WRECKED ON A ROCK" : "CRASHED INTO A PLANET");
                tumble(ship, contact.point, contact.normal, contact.speed);
            }
        }

        world.playerContacts.clear();
    }

    /** Hard knocks against the station's hull (which are also fined) or an outpost's damage the ship and set it tumbling. */
    inline void applyStationKnock(World& world, std::vector<combat::CombatEvent>& events)
    {
        for (const StationImpact* impact : {&world.stationImpact, &world.outpostImpact})
        {
            if (!impact->happened || impact->speed <= harmlessSpeed)
                continue;

            combat::damagePlayer(world, impactDamage(impact->speed), impact->point, events, impact == &world.stationImpact ? "WRECKED ON THE STATION" : "WRECKED ON THE OUTPOST");
            tumble(world.playerShip, impact->point, normalized(world.playerShip.position - impact->point), impact->speed);
        }
    }

    /** True for an NPC ship that is flying freely (not threading the station's slot, docked or gone). */
    inline bool solidNpc(const NpcShip& npc)
    {
        return npc.state == NpcState::Roaming || npc.state == NpcState::HeadingToStation || npc.state == NpcState::WarpingOut || npc.state == NpcState::Hostile;
    }

    /**
     * Rigid-body collisions between the player's ship and NPC ships, as spheres: they are pushed
     * apart, the impulse along the line between them is shared by their masses (with some bounce),
     * and both take damage from the closing speed and tumble (the player's ship; an NPC simply
     * takes the damage).
     */
    inline void collideShips(World& world, std::vector<combat::CombatEvent>& events)
    {
        Ship& player = world.playerShip;
        const float playerMass = shipTotalMass(player);

        for (int index = 0; index < static_cast<int>(world.npcShips.size()); ++index)
        {
            NpcShip& npc = world.npcShips[static_cast<std::size_t>(index)];

            if (!solidNpc(npc))
                continue;

            const Vec3 offset = player.position - npc.ship.position;
            const float distance = length(offset);
            const float contactDistance = shipRadius * 2.f;

            if (distance >= contactDistance)
                continue;

            const Vec3 normal = distance > 1e-3f ? offset / distance : Vec3{0.f, 1.f, 0.f};
            const float npcMass = shipTotalMass(npc.ship);
            const float totalMass = std::max(1.f, playerMass + npcMass);

            // Push apart, each by its share (the lighter one moves more).
            const float overlap = contactDistance - distance;
            player.position += normal * (overlap * npcMass / totalMass);
            npc.ship.position -= normal * (overlap * playerMass / totalMass);

            const float closing = -dot(player.velocity - npc.ship.velocity, normal);

            if (closing <= 0.f)
                continue;

            const float impulse = (1.f + shipRestitution) * closing / (1.f / playerMass + 1.f / std::max(1.f, npcMass));
            player.velocity += normal * (impulse / playerMass);
            npc.ship.velocity -= normal * (impulse / std::max(1.f, npcMass));

            if (player.cruiseEngaged)
                disengageCruise(player);

            const Vec3 point = npc.ship.position + normal * shipRadius;
            const float damage = impactDamage(closing);

            if (damage > 0.f)
            {
                combat::damagePlayer(world, damage, point, events, "WRECKED IN A COLLISION");
                combat::damageShip(world, index, damage, point, events);
                tumble(player, point, normal, closing);
            }
        }
    }

    /** One step of all of it, while the player is flying the ship (not the docking computer). */
    inline void update(World& world, float dt, std::vector<combat::CombatEvent>& events)
    {
        if (world.combat.player.destroyed)
        {
            world.playerContacts.clear();
            return;
        }

        applyContacts(world, events);
        applyStationKnock(world, events);
        collideShips(world, events);
        updateAir(world, dt, events);
    }

} // namespace impacts

#endif //DUSK_IMPACTS_H
