//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_WORLD_H
#define DUSK_WORLD_H

#include "objects/cube.h++"
#include "objects/planet.h++"
#include "objects/ship.h++"
#include "objects/collision_body.h++"
#include "systems/ship_physics.h++"
#include "tools/camera.h++"
#include "world/starfield.h++"
#include <vector>
#include "systems/orbital_physics.h++"
#include "objects/npc_ship.h++"
#include "systems/npc_ai.h++"
#include <cmath>

/** Owns all objects that exist in world coordinates. */
struct World {
    Starfield starfield;
    Cube cube;
    bool cubeActive = true;
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

/** Advances every NPC's simple-reflex behaviour and physics for one frame. */
inline void updateNpcShips(World& world, float dt)
{
    for (NpcShip& npc : world.npcShips)
    {
        const Vec3 gravity = gravityAt(world, npc.ship.position);

        npc_ai::updateNpcShip(
            npc,
            dt,
            world.npcRng,
            world.star,
            world.planets,
            world.cube.position,
            world.cubeActive,
            world.systemOuterRadius,
            gravity
        );
    }
}

/** Keeps the station cube circling its host planet's current (possibly moving) position. */
inline void updateStationOrbit(World& world, float dt)
{
    if (!world.cubeActive)
        return;

    if (world.stationHostPlanetIndex < 0 ||
        static_cast<std::size_t>(world.stationHostPlanetIndex) >= world.planets.size())
    {
        return;
    }

    world.stationOrbitAngle += world.stationOrbitSpeed * dt;

    const Planet& host = world.planets[static_cast<std::size_t>(world.stationHostPlanetIndex)];

    world.cube.position = host.position + Vec3
    {
        std::cos(world.stationOrbitAngle) * world.stationOrbitRadius,
        0.f,
        std::sin(world.stationOrbitAngle) * world.stationOrbitRadius
    };
}

/** Clears collision hits from every object before fresh detection runs. */
inline void clearWorldCollisions(World& world)
{
    world.playerShip.collision.clear();
    world.cube.collision.clear();

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

    if (world.cubeActive)
    {
        detectWorldSweptSphereCollision(
            world.playerShip.collision,
            CollisionObjectType::Ship,
            0,
            world.playerShip.previousPosition,
            world.playerShip.position,
            world.playerShip.collisionRadius,
            world.cube.collision,
            CollisionObjectType::Cube,
            0,
            world.cube.position,
            cubeCollisionRadius(world.cube)
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

        if (world.cubeActive)
        {
            detectWorldSphereCollision(
                world.cube.collision,
                CollisionObjectType::Cube,
                0,
                world.cube.position,
                cubeCollisionRadius(world.cube),
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

/** Advances world objects that have physics or animation. */
inline void updateWorldPhysics(World& world, float dt)
{
    const Vec3 shipGravity = gravityOnShip(world);
    integrateShipPhysics(world.playerShip, dt, shipGravity);

    orbital::integrateOrbitalPhysics(world.planets, world.star.position, world.star.mass, dt);
    updateStationOrbit(world, dt);
    updateNpcShips(world, dt);

    if (world.cubeActive)
        updateCube(world.cube, dt);

    updateWorldCollisions(world);
}

/** Updates camera-dependent world streaming after the camera has moved. */
inline void updateWorldStreaming(World& world, const Camera& camera)
{
    world.starfield.update(camera.position);
}

#endif //DUSK_WORLD_H
