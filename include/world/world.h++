//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_WORLD_H
#define DUSK_WORLD_H

#include "objects/cube.h++"
#include "objects/planet.h++"
#include "objects/ship.h++"
#include "systems/ship_physics.h++"
#include "tools/camera.h++"
#include "world/starfield.h++"
#include <vector>

/** Owns all objects that exist in world coordinates. */
struct World {
    /** Endless-looking background star volume. */
    Starfield starfield;

    /** Test cube in world space. */
    Cube cube;

    /** Whether the test cube should update and render in this scene. */
    bool cubeActive = true;

    /** Planet bodies visible in the current scene. */
    std::vector<Planet> planets;

    /** Singular player-controlled ship object in world space. */
    Ship playerShip;
};

/** Advances world objects that have physics or animation. */
inline void updateWorldPhysics(World& world, float dt)
{
    integrateShipPhysics(world.playerShip, dt);

    if (world.cubeActive)
        updateCube(world.cube, dt);
}

/** Updates camera-dependent world streaming after the camera has moved. */
inline void updateWorldStreaming(World& world, const Camera& camera)
{
    world.starfield.update(camera.position);
}

#endif //DUSK_WORLD_H
