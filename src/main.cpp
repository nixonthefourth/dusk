#include <iostream>
#include <SFML/Graphics.hpp>
#include "rendering/cube_renderer.h++"
#include "rendering/hud_renderer.h++"
#include "rendering/ship_renderer.h++"
#include "rendering/star_renderer.h++"
#include "scenes/default_scene.h++"
#include "scenes/scene_manager.h++"
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
    sceneManager.setScene<DefaultScene>();
    World& world = sceneManager.world();
    ShipInputState shipInputState;
    ShipCameraRig shipCameraRig;

    const ProjectionConfig projectionConfig = {1.f, world.starfield.radius()};
    const StarRenderer starRenderer({world.starfield.radius(), 1.f, 4.f}, projectionConfig);
    const CubeRenderer cubeRenderer(projectionConfig);
    const ShipRenderer shipRenderer(projectionConfig);
    const HudRenderer hudRenderer;
    updateShipCamera(camera, world.playerShip, 0.f, shipCameraRig);

    // Main update loop
    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();

        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape))
            window.close();

        updateShipFromKeyboard(world.playerShip, dt, shipInputState);
        sceneManager.activeScene().updatePhysics(dt);
        updateShipCamera(camera, world.playerShip, dt, shipCameraRig);
        sceneManager.activeScene().updateStreaming(camera);

        if (printClock.getElapsedTime().asSeconds() >= 0.25f)
        {
            std::cout
                << "Camera: ("
                << camera.position.x << ", "
                << camera.position.y << ", "
                << camera.position.z << ") yaw "
                << camera.yaw << " pitch "
                << camera.pitch << "\n";

            std::cout
                << "Ship: ("
                << world.playerShip.position.x << ", "
                << world.playerShip.position.y << ", "
                << world.playerShip.position.z << ") velocity "
                << shipSpeed(world.playerShip) << " throttle "
                << world.playerShip.throttle * 100.f
                << (world.playerShip.reverseThrust ? "% reverse\n" : "% forward\n");

            printClock.restart();
        }

        window.clear(sf::Color::Black);
        starRenderer.draw(window, world.starfield.stars(), camera);
        cubeRenderer.draw(window, world.cube, camera);
        shipRenderer.draw(window, world.playerShip, camera);
        hudRenderer.draw(window, world.playerShip);
        window.display();
    }

    return 0;
}
