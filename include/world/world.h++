//
// Created by Mykyta Khomiakov on 22/07/2026.
//
// The World: everything that exists in a system (star, planets, station, ships, asteroid belts,
// drifting rocks) plus the per-step update that moves it all: gravity, orbits, the station's
// orbit, NPCs, belts, collisions and contact responses.
//

#ifndef DUSK_WORLD_H
#define DUSK_WORLD_H

#include "objects/asteroid.h++"
#include "objects/cube.h++"
#include "objects/planet.h++"
#include "objects/ship.h++"
#include "objects/collision_body.h++"
#include "systems/ship_physics.h++"
#include "tools/camera.h++"
#include "procgen/asteroid_generation.h++"
#include "world/starfield.h++"
#include <vector>
#include "systems/orbital_physics.h++"
#include "objects/npc_ship.h++"
#include "systems/npc_ai.h++"
#include <algorithm>
#include <cmath>
#include <optional>
#include <random>

/** What kind of object the player has locked as a target. Only the station can be targeted for now. */
enum class TargetType { None, Station };

/** The player's current target lock. Room for an index once ships or planets become targetable. */
struct TargetLock {
    TargetType type = TargetType::None;
};

/** Owns all objects that exist in world coordinates. */
struct World {
    Starfield starfield;
    Station station;
    bool stationActive = true;
    std::vector<Planet> planets;

    /** All NPC ships currently populating this system. */
    std::vector<NpcShip> npcShips;

    /** Live randomness for NPC behaviour — reseeded fresh every time a system is entered. */
    std::mt19937 npcRng = std::mt19937(std::random_device{}());

    /** Outer bound NPCs roam within; set by whoever generates the system. */
    float systemOuterRadius = 40000.f;

    /** Central star. Always static — never touched by orbital integration. */
    Planet star;

    /** Index into `planets` that the station cube orbits, or -1 if there's no station. */
    int stationHostPlanetIndex = -1;
    float stationOrbitRadius = 0.f;
    float stationOrbitAngle = 0.f;
    float stationOrbitSpeed = 0.15f;

    Ship playerShip;

    /** What the player has targeted, shown on the scanner, compass and in-view brackets. */
    TargetLock target;

    /** Asteroid belts in this system: the star's belts, then planets' debris belts. Rocks are streamed on demand. */
    std::vector<AsteroidBelt> asteroidBelts;

    /** Lone rocks drifting through space near the player, spawned and recycled as you fly. */
    std::vector<Asteroid> driftingAsteroids;

    /** Shape templates for the drifting rocks: coarse ones first, then fine ones for boulders. */
    std::vector<AsteroidShape> looseRockShapes;
    int looseCoarseShapeCount = 0;

    /** Randomness for drifting-rock spawns (cosmetic, so not tied to the system seed). */
    std::mt19937 driftRng{0x5EEDu};

    /**
     * Seconds of simulated time in this system; drives belt orbits and tumbling rocks. A double,
     * because belt angles are recomputed from it every step and a float would start to step
     * visibly after a few hours.
     */
    double elapsedTime = 0.0;
};



/** Sums gravitational acceleration from the star and every planet at a given position. */
inline Vec3 gravityAt(const World& world, const Vec3& position)
{
    Vec3 acceleration = orbital::gravitationalAcceleration(position, world.star.position, world.star.mass);

    for (const Planet& planet : world.planets)
        acceleration += orbital::gravitationalAcceleration(position, planet.position, planet.mass);

    return acceleration;
}

/** Sums gravitational acceleration from the star and every planet at the ship's position. */
inline Vec3 gravityOnShip(const World& world)
{
    return gravityAt(world, world.playerShip.position);
}

/** Mass-lock zone around a planet or star: this fraction of its radius, plus a fixed pad, above the surface. */
constexpr float massLockRadiusFraction = 0.5f;
constexpr float massLockPadding = 5000.f;

/** Mass-lock radius around the station, measured from its centre. */
constexpr float stationMassLockRadius = 12000.f;

/**
 * Distance from `position` to the nearest mass-lock boundary (star, planets, station). Positive
 * means clear of every zone; zero or negative means mass-locked. This also sets how fast cruise
 * may go, so ships slow down smoothly on approach instead of slamming into the boundary.
 */
inline float cruiseMarginAt(const World& world, const Vec3& position)
{
    float margin = 1e30f;

    const auto checkBody = [&](const Planet& body)
    {
        if (body.radius <= 0.f)
            return;

        const float lockRadius = body.radius * (1.f + massLockRadiusFraction) + massLockPadding;
        margin = std::min(margin, length(position - body.position) - lockRadius);
    };

    if (world.star.isStar)
        checkBody(world.star);

    for (const Planet& planet : world.planets)
        checkBody(planet);

    if (world.stationActive)
        margin = std::min(margin, length(position - world.station.position) - stationMassLockRadius);

    return margin;
}

/**
 * Keeps a ship on the outside of planets and the star: if it has sunk into one, it is moved back
 * to the surface and loses the part of its velocity (relative to the body) that points inward.
 * Collision hits are still only recorded by updateWorldCollisions(); this is the minimal response
 * that stops the ship flying through a world it should be skimming.
 */
inline void resolveShipBodyContact(Ship& ship, const World& world)
{
    const auto resolve = [&](const Planet& body)
    {
        if (body.radius <= 0.f)
            return;

        const Vec3 offset = ship.position - body.position;
        const float distance = length(offset);
        const float contactDistance = body.radius + ship.collisionRadius;

        if (distance >= contactDistance)
            return;

        const Vec3 normal = distance > 0.f ? offset / distance : Vec3{0.f, 1.f, 0.f};
        ship.position = body.position + normal * contactDistance;

        const Vec3 relativeVelocity = ship.velocity - body.velocity;
        const float inwardSpeed = dot(relativeVelocity, normal);

        if (inwardSpeed < 0.f)
            ship.velocity -= normal * inwardSpeed;

        if (ship.cruiseEngaged)
            disengageCruise(ship);
    };

    // The default-constructed World carries a placeholder star (the menu scene uses it as a
    // backdrop around the ship); only a generated star is a solid body.
    if (world.star.isStar)
        resolve(world.star);

    for (const Planet& planet : world.planets)
        resolve(planet);
}

/** True while `position` is inside any asteroid belt's band (used for the HUD's field warning). */
inline bool insideAsteroidBelt(const World& world, const Vec3& position)
{
    return std::any_of(world.asteroidBelts.begin(), world.asteroidBelts.end(), [&](const AsteroidBelt& belt)
    {
        return procgen::beltDensityAt(belt, position) > 0.f;
    });
}

/**
 * Pushes a ship out of one rock and removes the part of its velocity, relative to the rock, that
 * points into it, so a moving rock shoves the ship along rather than passing through it. Rocks are
 * treated as far heavier than the ship. Contact drops the ship out of cruise.
 */
inline void resolveShipRockContact(Ship& ship, const Asteroid& rock)
{
    const Vec3 offset = ship.position - rock.position;
    const float distance = length(offset);
    const float contactDistance = rock.collisionRadius() + ship.collisionRadius;

    if (distance >= contactDistance)
        return;

    const Vec3 normal = distance > 0.f ? offset / distance : Vec3{0.f, 1.f, 0.f};
    ship.position = rock.position + normal * contactDistance;

    const float inwardSpeed = dot(ship.velocity - rock.velocity, normal);

    if (inwardSpeed < 0.f)
        ship.velocity -= normal * inwardSpeed;

    if (ship.cruiseEngaged)
        disengageCruise(ship);
}

/**
 * Keeps a ship outside every asteroid near it, belt rocks and drifting rocks alike. Belt rocks are
 * only generated within reach, and only when the ship is near a belt at all, so this is cheap.
 */
inline void resolveShipAsteroidContact(Ship& ship, const World& world)
{
    constexpr float reach = 3200.f; // largest rock radius plus the ship, with room to spare

    for (const AsteroidBelt& belt : world.asteroidBelts)
    {
        procgen::forEachAsteroidNear(belt, ship.position, reach, [&](const Asteroid& rock)
        {
            resolveShipRockContact(ship, rock);
        });
    }

    for (const Asteroid& rock : world.driftingAsteroids)
    {
        if (length(rock.position - ship.position) < reach)
            resolveShipRockContact(ship, rock);
    }
}

/** Moves each belt's centre to its star or host planet and turns it to its current orbital angle. */
inline void updateAsteroidBelts(World& world)
{
    constexpr double fullTurn = 6.283185307179586;

    for (AsteroidBelt& belt : world.asteroidBelts)
    {
        const bool onPlanet =
            belt.hostPlanetIndex >= 0 &&
            static_cast<std::size_t>(belt.hostPlanetIndex) < world.planets.size();

        const Planet& host = onPlanet ? world.planets[static_cast<std::size_t>(belt.hostPlanetIndex)] : world.star;
        belt.centre = host.position;
        belt.centreVelocity = host.velocity;

        // Recomputed from total elapsed time (not accumulated), so the angle never drifts.
        belt.rotation = static_cast<float>(std::fmod(static_cast<double>(belt.angularSpeed) * world.elapsedTime, fullTurn));
    }
}

/* ---- Drifting asteroids ------------------------------------------------------------------- */

/** How many lone rocks drift around the player, and the shell they live in. */
constexpr int driftingAsteroidCount = 40;
constexpr float driftInitialMinDistance = 8000.f;
constexpr float driftSpawnMinDistance = 33000.f; // just past the rock draw distance, so they fade in
constexpr float driftSpawnMaxDistance = 50000.f;
constexpr float driftRecycleDistance = 56000.f;

/** True if a rock of this radius could sit at `position` without being inside a body, a belt or the station. */
inline bool driftSpawnIsClear(const World& world, const Vec3& position, float radius)
{
    const auto clearOf = [&](const Planet& body)
    {
        return body.radius <= 0.f || length(position - body.position) > body.radius + radius + 6000.f;
    };

    if (world.star.isStar && !clearOf(world.star))
        return false;

    if (!std::all_of(world.planets.begin(), world.planets.end(), clearOf))
        return false;

    if (world.stationActive && length(position - world.station.position) < 8000.f)
        return false;

    return std::none_of(world.asteroidBelts.begin(), world.asteroidBelts.end(), [&](const AsteroidBelt& belt)
    {
        return procgen::distanceToBelt(belt, position) < 3000.f;
    });
}

/**
 * Spawns one drifting rock somewhere in the shell around the player. At normal speeds it is aimed
 * to pass by the player at a few thousand units, so lone rocks regularly sail across the view. In
 * cruise, rocks are seeded ahead along the flight path but kept off it, so they stream past
 * without being a hazard every few seconds.
 */
inline bool spawnDriftingAsteroid(World& world, Asteroid& rock, bool initial)
{
    std::mt19937& rng = world.driftRng;
    std::uniform_real_distribution<float> unit(0.f, 1.f);
    std::normal_distribution<float> gauss(0.f, 1.f);

    const Ship& ship = world.playerShip;
    const float shipSpeed = length(ship.velocity);
    const bool fast = shipSpeed > 2500.f;
    const Vec3 travel = shipSpeed > 0.f ? ship.velocity / shipSpeed : shipForward(ship);

    rock.radius = unit(rng) < 0.03f ? 1200.f + 800.f * unit(rng) : 70.f + 900.f * std::pow(unit(rng), 2.5f);

    for (int attempt = 0; attempt < 12; ++attempt)
    {
        Vec3 direction = normalized(Vec3{gauss(rng), gauss(rng) * 0.6f, gauss(rng)});

        if (fast)
        {
            direction = normalized(travel * 1.6f + direction);

            if (dot(direction, travel) < 0.4f)
                continue;
        }

        const float minDistance = initial ? driftInitialMinDistance : driftSpawnMinDistance;
        const float distance = minDistance + (driftSpawnMaxDistance - minDistance) * unit(rng);
        const Vec3 position = ship.position + direction * distance;

        if (!driftSpawnIsClear(world, position, rock.radius))
            continue;

        rock.position = position;

        const float speed = 30.f + 210.f * unit(rng);
        Vec3 heading;

        if (fast)
        {
            heading = normalized(Vec3{gauss(rng), gauss(rng) * 0.3f, gauss(rng)});
        }
        else
        {
            // Aim at a point a few thousand units off to one side of the player.
            Vec3 side = cross(ship.position - position, Vec3{gauss(rng), gauss(rng), gauss(rng)});
            side = length(side) > 0.f ? normalized(side) : Vec3{1.f, 0.f, 0.f};
            const Vec3 aim = ship.position + side * (3000.f + 12000.f * unit(rng));
            heading = normalized(aim - position);
        }

        rock.velocity = heading * speed;
        rock.shape = procgen::pickRockShape(
            rng,
            rock.radius,
            world.looseCoarseShapeCount,
            static_cast<int>(world.looseRockShapes.size())
        );
        procgen::rollRockSpin(rng, rock);
        return true;
    }

    return false;
}

/**
 * Keeps a population of lone rocks drifting around the player: tops it up, moves each one along
 * its heading, and recycles any that wander out of range or into a planet or the star.
 */
inline void updateDriftingAsteroids(World& world, float dt)
{
    if (world.looseRockShapes.empty())
        return;

    const bool firstFill = world.driftingAsteroids.empty();

    while (static_cast<int>(world.driftingAsteroids.size()) < driftingAsteroidCount)
    {
        Asteroid rock;

        if (!spawnDriftingAsteroid(world, rock, firstFill))
            break;

        world.driftingAsteroids.push_back(rock);
    }

    for (Asteroid& rock : world.driftingAsteroids)
    {
        rock.position += rock.velocity * dt;

        const bool outOfRange = length(rock.position - world.playerShip.position) > driftRecycleDistance;

        // A rock that drifts into a planet or the star is gone; it is otherwise free to sail
        // through belts and past the station like anything else.
        const auto inside = [&](const Planet& body)
        {
            return body.radius > 0.f && length(rock.position - body.position) < body.radius + rock.radius;
        };

        const bool hitBody =
            (world.star.isStar && inside(world.star)) ||
            std::any_of(world.planets.begin(), world.planets.end(), inside);

        if (outOfRange || hitBody)
            spawnDriftingAsteroid(world, rock, false);
    }
}



inline Vec3 stationVelocity(const World& world); // defined below, with the station's orbit

/** Advances every NPC's behaviour and physics, handing each the station's docking slot as it is this step. */
inline void updateNpcShips(World& world, float dt)
{
    npc_ai::StationDockingInfo dock;
    dock.exists = world.stationActive;

    if (dock.exists)
    {
        dock.mouth = stationDockMouth(world.station);
        dock.normal = stationDockNormal(world.station);
        dock.slotAxis = stationDockSlotAxis(world.station);
        dock.velocity = stationVelocity(world);
    }

    for (NpcShip& npc : world.npcShips)
    {
        const Vec3 gravity = gravityAt(world, npc.ship.position);
        npc.ship.cruiseMargin = cruiseMarginAt(world, npc.ship.position);

        npc_ai::updateNpcShip(
            npc,
            dt,
            world.npcRng,
            world.star,
            world.planets,
            dock,
            world.systemOuterRadius,
            gravity
        );

        if (npc.isVisible())
            resolveShipBodyContact(npc.ship, world);
    }
}

/**
 * Direction the station is travelling along its orbit. The docking slot faces this way, so the
 * approach lane in front of it runs alongside the orbit and never points into the host planet
 * or back toward the star.
 */
inline Vec3 stationOrbitTangent(const World& world)
{
    const float direction = world.stationOrbitSpeed < 0.f ? -1.f : 1.f;
    return Vec3
    {
        -std::sin(world.stationOrbitAngle) * direction,
        0.f,
        std::cos(world.stationOrbitAngle) * direction
    };
}

/** Keeps the station circling its host planet's current (possibly moving) position. */
inline void updateStationOrbit(World& world, float dt)
{
    if (!world.stationActive)
        return;

    if (world.stationHostPlanetIndex < 0 ||
        static_cast<std::size_t>(world.stationHostPlanetIndex) >= world.planets.size())
    {
        return;
    }

    world.stationOrbitAngle += world.stationOrbitSpeed * dt;

    const Planet& host = world.planets[static_cast<std::size_t>(world.stationHostPlanetIndex)];

    world.station.position = host.position + Vec3
    {
        std::cos(world.stationOrbitAngle) * world.stationOrbitRadius,
        0.f,
        std::sin(world.stationOrbitAngle) * world.stationOrbitRadius
    };

    world.station.dockFacing = stationOrbitTangent(world);
}

/** World-space velocity of the station: its host planet's orbital velocity plus its own orbit around the host. */
inline Vec3 stationVelocity(const World& world)
{
    if (!world.stationActive ||
        world.stationHostPlanetIndex < 0 ||
        static_cast<std::size_t>(world.stationHostPlanetIndex) >= world.planets.size())
    {
        return {};
    }

    const Planet& host = world.planets[static_cast<std::size_t>(world.stationHostPlanetIndex)];
    return host.velocity + stationOrbitTangent(world) * (std::abs(world.stationOrbitSpeed) * world.stationOrbitRadius);
}

/** True if the current target still exists (a station can vanish when the system changes). */
inline bool hasValidTarget(const World& world)
{
    switch (world.target.type)
    {
        case TargetType::Station: return world.stationActive;
        case TargetType::None: break;
    }

    return false;
}

/** World position of the current target, if there is one. */
inline std::optional<Vec3> targetPosition(const World& world)
{
    if (!hasValidTarget(world))
        return std::nullopt;

    return world.station.position;
}

/** World velocity of the current target (zero if there is none). */
inline Vec3 targetVelocity(const World& world)
{
    return hasValidTarget(world) ? stationVelocity(world) : Vec3{};
}

/** Short HUD label for the current target. */
inline const char* targetLabel(const World& world)
{
    return hasValidTarget(world) ? "STATION" : "";
}

/** Steps the target lock to the next available target, wrapping back to none. */
inline void cycleTarget(World& world)
{
    if (world.target.type == TargetType::None && world.stationActive)
        world.target.type = TargetType::Station;
    else
        world.target.type = TargetType::None;
}

/** Clears collision hits from every object before fresh detection runs. */
inline void clearWorldCollisions(World& world)
{
    world.playerShip.collision.clear();
    world.station.collision.clear();

    for (Planet& planet : world.planets)
        planet.collision.clear();
}

/** Adds a symmetric hit record to both objects in a collision pair. */
inline void addWorldCollisionPair(
    CollisionBody& firstBody,
    CollisionObjectType firstType,
    std::size_t firstIndex,
    CollisionBody& secondBody,
    CollisionObjectType secondType,
    std::size_t secondIndex,
    const Vec3& firstToSecondNormal,
    float penetration
)
{
    addCollisionHit(firstBody, secondType, secondIndex, firstToSecondNormal, penetration);
    addCollisionHit(secondBody, firstType, firstIndex, firstToSecondNormal * -1.f, penetration);
}

/** Registers a spherical collision pair if both object-level bodies allow detection. */
inline void detectWorldSphereCollision(
    CollisionBody& firstBody,
    CollisionObjectType firstType,
    std::size_t firstIndex,
    const Vec3& firstPosition,
    float firstRadius,
    CollisionBody& secondBody,
    CollisionObjectType secondType,
    std::size_t secondIndex,
    const Vec3& secondPosition,
    float secondRadius
)
{
    if (!firstBody.detectsCollisions || !secondBody.detectsCollisions)
        return;

    Vec3 normal;
    float penetration = 0.f;

    if (!detectSphereCollision(firstPosition, firstRadius, secondPosition, secondRadius, normal, penetration))
        return;

    addWorldCollisionPair(
        firstBody,
        firstType,
        firstIndex,
        secondBody,
        secondType,
        secondIndex,
        normal,
        penetration
    );
}

/** Registers a collision when a moving object crossed another body's sphere this frame. */
inline void detectWorldSweptSphereCollision(
    CollisionBody& movingBody,
    CollisionObjectType movingType,
    std::size_t movingIndex,
    const Vec3& movingStart,
    const Vec3& movingEnd,
    float movingRadius,
    CollisionBody& staticBody,
    CollisionObjectType staticType,
    std::size_t staticIndex,
    const Vec3& staticPosition,
    float staticRadius
)
{
    if (!movingBody.detectsCollisions || !staticBody.detectsCollisions)
        return;

    Vec3 normal;
    float penetration = 0.f;

    if (!detectSweptSphereCollision(
        movingStart,
        movingEnd,
        movingRadius,
        staticPosition,
        staticRadius,
        normal,
        penetration
    ))
    {
        return;
    }

    addWorldCollisionPair(
        movingBody,
        movingType,
        movingIndex,
        staticBody,
        staticType,
        staticIndex,
        normal,
        penetration
    );
}

/** Refreshes object-level collision hits for every rigid world object. */
inline void updateWorldCollisions(World& world)
{
    clearWorldCollisions(world);

    if (world.stationActive)
    {
        detectWorldSweptSphereCollision(
            world.playerShip.collision,
            CollisionObjectType::Ship,
            0,
            world.playerShip.previousPosition,
            world.playerShip.position,
            world.playerShip.collisionRadius,
            world.station.collision,
            CollisionObjectType::Cube,
            0,
            world.station.position,
            stationCollisionRadius(world.station)
        );
    }

    for (std::size_t planetIndex = 0; planetIndex < world.planets.size(); ++planetIndex)
    {
        Planet& planet = world.planets[planetIndex];

        detectWorldSweptSphereCollision(
            world.playerShip.collision,
            CollisionObjectType::Ship,
            0,
            world.playerShip.previousPosition,
            world.playerShip.position,
            world.playerShip.collisionRadius,
            planet.collision,
            CollisionObjectType::Planet,
            planetIndex,
            planet.position,
            planetCollisionRadius(planet)
        );

        if (world.stationActive)
        {
            detectWorldSphereCollision(
                world.station.collision,
                CollisionObjectType::Cube,
                0,
                world.station.position,
                stationCollisionRadius(world.station),
                planet.collision,
                CollisionObjectType::Planet,
                planetIndex,
                planet.position,
                planetCollisionRadius(planet)
            );
        }
    }

    for (std::size_t firstIndex = 0; firstIndex < world.planets.size(); ++firstIndex)
    {
        for (std::size_t secondIndex = firstIndex + 1; secondIndex < world.planets.size(); ++secondIndex)
        {
            detectWorldSphereCollision(
                world.planets[firstIndex].collision,
                CollisionObjectType::Planet,
                firstIndex,
                world.planets[firstIndex].position,
                planetCollisionRadius(world.planets[firstIndex]),
                world.planets[secondIndex].collision,
                CollisionObjectType::Planet,
                secondIndex,
                world.planets[secondIndex].position,
                planetCollisionRadius(world.planets[secondIndex])
            );
        }
    }
}

/** Advances world objects that have physics or animation. Autopilots can take over the player ship. */
inline void updateWorldPhysics(World& world, float dt, bool integratePlayerShip = true)
{
    world.elapsedTime += dt;

    // Refreshed even under autopilot, so the HUD always knows whether cruise is available.
    world.playerShip.cruiseMargin = cruiseMarginAt(world, world.playerShip.position);

    if (integratePlayerShip)
    {
        const Vec3 shipGravity = gravityOnShip(world);
        integrateShipPhysics(world.playerShip, dt, shipGravity);
        resolveShipBodyContact(world.playerShip, world);
    }

    orbital::integrateOrbitalPhysics(world.planets, world.star.position, world.star.mass, dt);

    // Belts ride with their star or planet; rocks are then resolved against the ship where they
    // actually are this step.
    updateAsteroidBelts(world);
    updateDriftingAsteroids(world, dt);

    if (integratePlayerShip)
        resolveShipAsteroidContact(world.playerShip, world);
    updateStationOrbit(world, dt);
    updateNpcShips(world, dt);

    if (world.stationActive)
        updateStation(world.station, dt);

    updateWorldCollisions(world);
}

/** Updates camera-dependent world streaming after the camera has moved. */
inline void updateWorldStreaming(World& world, const Camera& camera)
{
    world.starfield.update(camera.position);
}

#endif //DUSK_WORLD_H
