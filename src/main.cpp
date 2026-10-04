#include <algorithm>
#include <cmath>
#include <iostream>
#include <SFML/Graphics.hpp>
#include "rendering/station_renderer.h++"
#include "rendering/hud_renderer.h++"
#include "rendering/planet_renderer.h++"
#include "rendering/ship_renderer.h++"
#include "rendering/star_renderer.h++"
#include "scenes/scene_manager.h++"
#include "scenes/main_menu.h++"
#include "tools/camera.h++"
#include "tools/ship_controller.h++"
#include "procgen/galaxy.h++"
#include "scenes/system_scene.h++"

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

    const ProjectionConfig projectionConfig = {1.f, sceneManager.world().starfield.radius()};

    // Stars and planets are hundreds of thousands of units away, far past the starfield-sized far
    // plane that ships and stations use; they get their own, covering the whole system.
    const ProjectionConfig bodyProjectionConfig = {1.f, 10000000.f};

    const StarRenderer starRenderer({sceneManager.world().starfield.radius(), 1.f, 4.f}, projectionConfig);
    const PlanetRenderer planetRenderer(bodyProjectionConfig);
    const StationRenderer stationRenderer(projectionConfig);
    const ShipRenderer shipRenderer(projectionConfig);
    const HudRenderer hudRenderer;

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

        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
            {
                if (keyPressed->code == sf::Keyboard::Key::Escape)
                    window.close();
            }

            sceneManager.activeScene().handleEvent(*event, window);
        }

        applySceneTransition(sceneManager.activeScene().consumeTransition());

        if (!window.isOpen())
            continue;

        World& world = sceneManager.world();

        if (sceneManager.activeScene().acceptsShipInput())
            updateShipFromKeyboard(world.playerShip, dt, shipInputState);

        // Physics runs in sub-steps no longer than maxPhysicsStep, so integration and the flight
        // computer behave the same whatever the frame rate.
        constexpr float maxPhysicsStep = 1.f / 120.f;
        const int physicsSteps = std::max(1, static_cast<int>(std::ceil(dt / maxPhysicsStep)));
        const float physicsDt = dt / static_cast<float>(physicsSteps);

        for (int step = 0; step < physicsSteps; ++step)
            sceneManager.activeScene().updatePhysics(physicsDt);
        sceneManager.activeScene().updateCamera(camera, dt, shipCameraRig);
        sceneManager.activeScene().updateStreaming(camera);
        applySceneTransition(sceneManager.activeScene().consumeTransition());

        World& renderWorld = sceneManager.world();

        window.clear(sf::Color::Black);
        starRenderer.draw(window, renderWorld.starfield.stars(), camera);
        planetRenderer.drawSystem(window, renderWorld.star, renderWorld.planets, camera);

        if (renderWorld.stationActive)
            stationRenderer.draw(window, renderWorld.station, camera);

        shipRenderer.draw(window, renderWorld.playerShip, camera);

        for (const NpcShip& npc : renderWorld.npcShips)
        {
            if (npc.isVisible())
                shipRenderer.draw(window, npc.ship, camera);
        }

        if (sceneManager.activeScene().showsHud())
        {
            hudRenderer.drawFlightMarkers(window, renderWorld.playerShip, camera);
            hudRenderer.draw(window, renderWorld.playerShip);
        }

        sceneManager.activeScene().drawOverlay(window);
        window.display();
    }

    return 0;
}
