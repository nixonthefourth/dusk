//
// Created by Mykyta Khomiakov on 24/07/2026.
//

#ifndef DUSK_SCENE_H
#define DUSK_SCENE_H

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
#include "tools/camera.h++"
#include "tools/ship_controller.h++"
#include "world/world.h++"

/** Scene switch requests emitted by interactive scenes. */
enum class SceneTransition {
    None,
    FirstTest,
    TwoPlanet,
    Exit
};

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

    /** Handles event-driven scene input such as menu clicks. */
    virtual void handleEvent(const sf::Event&, const sf::RenderWindow&)
    {
    }

    /** Whether the scene should receive live ship keyboard controls. */
    virtual bool acceptsShipInput() const
    {
        return true;
    }

    /** Whether the standard ship HUD should render over this scene. */
    virtual bool showsHud() const
    {
        return true;
    }

    /** Advances world physics and scene-specific simulation. */
    virtual void updatePhysics(float dt)
    {
        updateWorldPhysics(world(), dt);
    }

    /** Updates the active camera for this scene. */
    virtual void updateCamera(Camera& camera, float dt, ShipCameraRig& rig)
    {
        updateShipCamera(camera, world().playerShip, dt, rig);
    }

    /** Updates camera-dependent streaming such as recycled starfields. */
    virtual void updateStreaming(const Camera& camera)
    {
        updateWorldStreaming(world(), camera);
    }

    /** Draws screen-space UI after the 3D world has rendered. */
    virtual void drawOverlay(sf::RenderTarget&)
    {
    }

    /** Returns and clears any scene switch requested since the previous frame. */
    virtual SceneTransition consumeTransition()
    {
        return SceneTransition::None;
    }
};

#endif //DUSK_SCENE_H
