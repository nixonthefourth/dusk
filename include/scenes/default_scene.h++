//
// Created by Mykyta Khomiakov on 24/07/2026.
//

#ifndef DUSK_DEFAULT_SCENE_H
#define DUSK_DEFAULT_SCENE_H

#include "scenes/scene.h++"

/** The default scene used by the game while experimenting with ships and projection. */
class DefaultScene : public Scene {
public:
    /** Builds the default world and leaves the ship on its built-in vector model. */
    DefaultScene()
    {
        world_.cube.position = {0.f, 0.f, 4000.f};

        // To use an OBJ ship model instead, keep the OBJ with the project and uncomment:
        //
        // ObjLoadOptions options;
        // options.scale = 20.f;
        // options.flipY = true;
        // world_.playerShip.loadObjModel("../assets/ships/ship.obj", options);
    }

    const char* name() const override
    {
        return "default";
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

#endif //DUSK_DEFAULT_SCENE_H
