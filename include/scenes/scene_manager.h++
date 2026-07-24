//
// Created by Mykyta Khomiakov on 24/07/2026.
//

#ifndef DUSK_SCENE_MANAGER_H
#define DUSK_SCENE_MANAGER_H

#include "scenes/scene.h++"
#include <memory>
#include <utility>

/** Owns and exposes the active scene. */
class SceneManager {
public:
    /** Constructs and activates a scene type. */
    template <typename SceneType, typename... Args>
    void setScene(Args&&... args)
    {
        activeScene_ = std::make_unique<SceneType>(std::forward<Args>(args)...);
    }

    /** Returns the current active scene. */
    Scene& activeScene()
    {
        return *activeScene_;
    }

    /** Returns the current active world. */
    World& world()
    {
        return activeScene().world();
    }

private:
    std::unique_ptr<Scene> activeScene_;
};

#endif //DUSK_SCENE_MANAGER_H
