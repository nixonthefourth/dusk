//
// Created by Mykyta Khomiakov on 24/07/2026.
//

#ifndef DUSK_FIRST_TEST_SCENE_H
#define DUSK_FIRST_TEST_SCENE_H

#include "scenes/scene.h++"

/** First playable test: fly toward the cube portal beside a planet. */
class FirstTestScene : public Scene {
public:
    FirstTestScene()
    {
        world_.cubeActive = true;
        world_.cube.position = {0.f, 0.f, 3600.f};
        world_.cube.size = 780.f;
        world_.cube.rotationSpeed = 0.86f;

        world_.playerShip.position = {0.f, -60.f, 0.f};
        world_.playerShip.velocity = {};
        world_.playerShip.yaw = 0.f;
        world_.playerShip.pitch = 0.f;
        world_.playerShip.throttle = 0.f;
        world_.playerShip.reverseThrust = false;

        Planet planet;
        planet.position = {-2600.f, -900.f, 6200.f};
        planet.radius = 1800.f;
        world_.planets.push_back(planet);
    }

    const char* name() const override
    {
        return "first test";
    }

    World& world() override
    {
        return world_;
    }

    const World& world() const override
    {
        return world_;
    }

    void updatePhysics(float dt) override
    {
        Scene::updatePhysics(dt);

        if (world_.playerShip.collision.collidingWith(CollisionObjectType::Cube))
            pendingTransition_ = SceneTransition::TwoPlanet;
    }

    SceneTransition consumeTransition() override
    {
        const SceneTransition transition = pendingTransition_;
        pendingTransition_ = SceneTransition::None;
        return transition;
    }

private:
    World world_;
    SceneTransition pendingTransition_ = SceneTransition::None;
};

#endif //DUSK_FIRST_TEST_SCENE_H
