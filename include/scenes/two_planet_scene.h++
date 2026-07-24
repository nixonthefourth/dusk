//
// Created by Mykyta Khomiakov on 24/07/2026.
//

#ifndef DUSK_TWO_PLANET_SCENE_H
#define DUSK_TWO_PLANET_SCENE_H

#include "scenes/scene.h++"

/** Second playable test: a clear space scene with two projected planets. */
class TwoPlanetScene : public Scene {
public:
    TwoPlanetScene()
    {
        world_.cubeActive = false;

        world_.playerShip.position = {0.f, -80.f, 0.f};
        world_.playerShip.velocity = {};
        world_.playerShip.yaw = 0.f;
        world_.playerShip.pitch = 0.f;
        world_.playerShip.throttle = 0.f;
        world_.playerShip.reverseThrust = false;

        Planet bluePlanet;
        bluePlanet.position = {-2400.f, -850.f, 5600.f};
        bluePlanet.radius = 1650.f;
        bluePlanet.hasRing = true;
        bluePlanet.ringRotationDegrees = -12.f;
        bluePlanet.ringFlattening = 0.22f;
        world_.planets.push_back(bluePlanet);

        Planet emberPlanet;
        emberPlanet.position = {2600.f, 650.f, 7600.f};
        emberPlanet.radius = 1200.f;
        world_.planets.push_back(emberPlanet);
    }

    const char* name() const override
    {
        return "two planet";
    }

    World& world() override
    {
        return world_;
    }

    const World& world() const override
    {
        return world_;
    }

private:
    World world_;
};

#endif //DUSK_TWO_PLANET_SCENE_H
