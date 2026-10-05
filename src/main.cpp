//
// Entry point and main loop: creates the window, scene manager and renderers, then every frame
// handles events, applies ship input, runs physics in bounded sub-steps, updates the camera and
// streaming, and draws the world, travel effects, HUD and scene overlay in that order.
//

#include <algorithm>
#include <cmath>
#include <iostream>
#include <SFML/Graphics.hpp>
#include "rendering/station_renderer.h++"
#include "rendering/hud_renderer.h++"
#include "rendering/asteroid_renderer.h++"
#include "rendering/travel_effects_renderer.h++"
#include "ui/style.h++"
#include "rendering/planet_renderer.h++"
#include "rendering/ship_renderer.h++"
#include "rendering/star_renderer.h++"
#include "scenes/scene_manager.h++"
#include "scenes/main_menu.h++"
#include "tools/camera.h++"
#include "tools/ship_controller.h++"
#include "procgen/galaxy.h++"
#include "scenes/system_scene.h++"

/** Builds the window, scenes and renderers, then runs the game loop until the window closes. */
int main() {
    sf::RenderWindow window(
        sf::VideoMode({800, 600}),
        "dusk"
    );
    window.setVerticalSyncEnabled(true);

    // Start clock
    sf::Clock clock;
    sf::Clock printClock;

    // Initialise Camera
    Camera camera;

    // Initialise scene/world
    Galaxy galaxy = generateGalaxy(1337u); // TODO: seed from a save file or menu input later

    SceneManager sceneManager;
    sceneManager.setScene<MainMenuScene>();
    ShipInputState shipInputState;
    ShipCameraRig shipCameraRig;

    // Renderers. There is no depth buffer: everything is drawn back to front in a fixed order
    // (see the bottom of the loop), and each renderer only reads the world.
    //
    // Ships, stations, rocks and stars share a far plane the size of the starfield.
    const ProjectionConfig projectionConfig = {1.f, sceneManager.world().starfield.radius()};

    // Stars and planets are hundreds of thousands of units away, far past the starfield-sized far
    // plane that ships and stations use; they get their own, covering the whole system.
    const ProjectionConfig bodyProjectionConfig = {1.f, 10000000.f};

    const StarRenderer starRenderer({sceneManager.world().starfield.radius(), 1.f, 4.f}, projectionConfig);
    const PlanetRenderer planetRenderer(bodyProjectionConfig);

    // Rocks are only drawn near the camera; belt dust spans the whole system like the planets.
    const AsteroidRenderer asteroidRenderer(projectionConfig, bodyProjectionConfig);
    const TravelEffectsRenderer travelEffectsRenderer(projectionConfig);
    const StationRenderer stationRenderer(projectionConfig);
    const ShipRenderer shipRenderer(projectionConfig);
    const HudRenderer hudRenderer;

    // Swaps scenes when the active one asks to (PLAY on the menu, EXIT), and resets per-scene
    // state that lives out here: edge-triggered key tracking and the camera spring.
    auto applySceneTransition = [&](SceneTransition transition) {
        if (transition == SceneTransition::None)
            return;

        switch (transition)
        {
            case SceneTransition::EnterSystem:
                sceneManager.setScene<SystemScene>(galaxy, 0);
                break;

            case SceneTransition::Exit:
                window.close();
                return;

            case SceneTransition::None:
                break;
        }

        shipInputState = {};
        shipCameraRig = {};
        sceneManager.activeScene().updateCamera(camera, 0.f, shipCameraRig);
        sceneManager.activeScene().updateStreaming(camera);
    };

    sceneManager.activeScene().updateCamera(camera, 0.f, shipCameraRig);
    sceneManager.activeScene().updateStreaming(camera);

    // Main update loop
    while (window.isOpen()) {
        // A stalled frame (window drag, breakpoint, alt-tab) is capped rather than simulated in one
        // enormous step that would fling the ship or the planets.
        constexpr float maxFrameTime = 0.1f;
        const float dt = std::min(clock.restart().asSeconds(), maxFrameTime);

        // Events: closing the window, Escape (unless the scene wants it, e.g. to close a map), and
        // everything else straight to the active scene.
        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
            {
                if (keyPressed->code == sf::Keyboard::Key::Escape && !sceneManager.activeScene().capturesEscape())
                    window.close();
            }

            sceneManager.activeScene().handleEvent(*event, window);
        }

        applySceneTransition(sceneManager.activeScene().consumeTransition());

        if (!window.isOpen())
            continue;

        World& world = sceneManager.world();

        // Held keys become pilot intent (throttle, turn demands, toggles), unless the scene has the
        // keyboard (a map is open, the docking computer or a hyperspace jump is flying).
        if (sceneManager.activeScene().acceptsShipInput())
            updateShipFromKeyboard(world.playerShip, dt, shipInputState);

        // Physics runs in sub-steps no longer than maxPhysicsStep, so integration and the flight
        // computer behave the same whatever the frame rate.
        constexpr float maxPhysicsStep = 1.f / 120.f;
        const int physicsSteps = std::max(1, static_cast<int>(std::ceil(dt / maxPhysicsStep)));
        const float physicsDt = dt / static_cast<float>(physicsSteps);

        for (int step = 0; step < physicsSteps; ++step)
            sceneManager.activeScene().updatePhysics(physicsDt);

        // The camera follows the ship once per frame (it smooths internally), then the starfield
        // re-wraps around the camera's new position.
        sceneManager.activeScene().updateCamera(camera, dt, shipCameraRig);
        sceneManager.activeScene().updateStreaming(camera);
        applySceneTransition(sceneManager.activeScene().consumeTransition());

        World& renderWorld = sceneManager.world();

        // Draw order is depth order: distant stars, then stellar bodies, rocks, the station and
        // ships, then the travel effects, the HUD, and finally the scene's screen-space overlay.
        window.clear(style::background);
        starRenderer.draw(window, renderWorld.starfield.stars(), camera);
        planetRenderer.drawSystem(window, renderWorld.star, renderWorld.planets, camera);
        asteroidRenderer.draw(window, renderWorld, camera);

        if (renderWorld.stationActive)
            stationRenderer.draw(window, renderWorld.station, camera);

        shipRenderer.draw(window, renderWorld.playerShip, camera);

        for (const NpcShip& npc : renderWorld.npcShips)
        {
            if (npc.isVisible())
                shipRenderer.draw(window, npc.ship, camera);
        }

        // Cruise and hyperspace animations sit over the 3D view and under the HUD; the hyperspace
        // tunnel covers the whole view.
        travelEffectsRenderer.draw(window, renderWorld, camera, sceneManager.activeScene().travelEffects());

        if (sceneManager.activeScene().showsHud())
            hudRenderer.draw(window, renderWorld, camera);

        sceneManager.activeScene().drawOverlay(window);
        window.display();
    }

    return 0;
}
