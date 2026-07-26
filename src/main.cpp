#include <iostream>
#include <SFML/Graphics.hpp>
#include "rendering/cube_renderer.h++"
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
    const StarRenderer starRenderer({sceneManager.world().starfield.radius(), 1.f, 4.f}, projectionConfig);
    const PlanetRenderer planetRenderer(projectionConfig);
    const CubeRenderer cubeRenderer(projectionConfig);
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

        std::cout << "Scene: " << sceneManager.activeScene().name() << "\n";
    };

    sceneManager.activeScene().updateCamera(camera, 0.f, shipCameraRig);
    sceneManager.activeScene().updateStreaming(camera);

    // Main update loop
    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();

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

        sceneManager.activeScene().updatePhysics(dt);
        sceneManager.activeScene().updateCamera(camera, dt, shipCameraRig);
        sceneManager.activeScene().updateStreaming(camera);
        applySceneTransition(sceneManager.activeScene().consumeTransition());

        World& renderWorld = sceneManager.world();

        window.clear(sf::Color::Black);
        starRenderer.draw(window, renderWorld.starfield.stars(), camera);
        planetRenderer.drawSystem(window, renderWorld.star, renderWorld.planets, camera);

        if (renderWorld.cubeActive)
            cubeRenderer.draw(window, renderWorld.cube, camera);

        shipRenderer.draw(window, renderWorld.playerShip, camera);

        if (sceneManager.activeScene().showsHud())
            hudRenderer.draw(window, renderWorld.playerShip);

        sceneManager.activeScene().drawOverlay(window);
        window.display();
    }

    return 0;
}
