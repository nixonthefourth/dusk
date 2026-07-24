//
// Created by Mykyta Khomiakov on 24/07/2026.
//

#ifndef DUSK_SCENE_H
#define DUSK_SCENE_H

#include "tools/camera.h++"
#include "world/world.h++"

/** Base interface for a self-contained playable scene. */
class Scene {
public:
    virtual ~Scene() = default;

    /** Human-readable scene name for logs or debug HUDs. */
    virtual const char* name() const = 0;

    /** Mutable world owned by the scene. */
    virtual World& world() = 0;

    /** Read-only world owned by the scene. */
    virtual const World& world() const = 0;

    /** Advances world physics and scene-specific simulation. */
    virtual void updatePhysics(float dt)
    {
        updateWorldPhysics(world(), dt);
    }

    /** Updates camera-dependent streaming such as recycled starfields. */
    virtual void updateStreaming(const Camera& camera)
    {
        updateWorldStreaming(world(), camera);
    }
};

#endif //DUSK_SCENE_H
