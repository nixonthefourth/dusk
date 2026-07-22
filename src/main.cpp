#include <iostream>
#include <SFML/Graphics.hpp>
#include "objects/cube.h++"
#include "rendering/cube_renderer.h++"
#include "rendering/star_renderer.h++"
#include "tools/camera_controller.h++"
#include "tools/camera.h++"
#include "world/starfield.h++"

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
    CameraControls cameraControls;

    // Initialise world
    const StarfieldConfig starfieldConfig;
    Starfield starfield(starfieldConfig);
    Cube cube;

    const ProjectionConfig projectionConfig = {1.f, starfieldConfig.radius};
    const StarRenderer starRenderer({starfieldConfig.radius, 1.f, 4.f}, projectionConfig);
    const CubeRenderer cubeRenderer(projectionConfig);

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

        updateCameraFromKeyboard(camera, dt, cameraControls);
        starfield.update(camera.position);
        updateCube(cube, dt);

        if (printClock.getElapsedTime().asSeconds() >= 0.25f)
        {
            Vec3 relativeCube = cube.position - camera.position;

            std::cout
                << "Camera: ("
                << camera.position.x << ", "
                << camera.position.y << ", "
                << camera.position.z << ") yaw "
                << camera.yaw << " pitch "
                << camera.pitch << "\n";

            std::cout
                << "Cube relative: ("
                << relativeCube.x << ", "
                << relativeCube.y << ", "
                << relativeCube.z << ")\n";

            printClock.restart();
        }

        window.clear(sf::Color::Black);
        starRenderer.draw(window, starfield.stars(), camera);
        cubeRenderer.draw(window, cube, camera);
        window.display();
    }

    return 0;
}
