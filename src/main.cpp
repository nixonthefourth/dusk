#include <iostream>
#include <SFML/Graphics.hpp>
#include "rendering/cube_renderer.h++"
#include "rendering/hud_renderer.h++"
#include "rendering/planet_renderer.h++"
#include "rendering/ship_renderer.h++"
#include "rendering/star_renderer.h++"
#include "scenes/first_test_scene.h++"
#include "scenes/scene_manager.h++"
#include "scenes/main_menu.h++"
#include "scenes/two_planet_scene.h++"
#include "tools/camera.h++"
#include "tools/ship_controller.h++"

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
            case SceneTransition::FirstTest:
                sceneManager.setScene<FirstTestScene>();
                break;

            case SceneTransition::TwoPlanet:
                sceneManager.setScene<TwoPlanetScene>();
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

        if (printClock.getElapsedTime().asSeconds() >= 0.25f)
        {
            std::cout
                << "Scene: " << sceneManager.activeScene().name() << "\n"
                << "Camera: ("
                << camera.position.x << ", "
                << camera.position.y << ", "
                << camera.position.z << ") yaw "
                << camera.yaw << " pitch "
                << camera.pitch << "\n";

            std::cout
                << "Ship: ("
                << renderWorld.playerShip.position.x << ", "
                << renderWorld.playerShip.position.y << ", "
                << renderWorld.playerShip.position.z << ") velocity "
                << shipSpeed(renderWorld.playerShip) << " throttle "
                << renderWorld.playerShip.throttle * 100.f
                << (renderWorld.playerShip.reverseThrust ? "% reverse\n" : "% forward\n");

            printClock.restart();
        }

        window.clear(sf::Color::Black);
        starRenderer.draw(window, renderWorld.starfield.stars(), camera);
        planetRenderer.draw(window, renderWorld.planets, camera);

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
