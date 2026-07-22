#include <iostream>
#include <SFML/Graphics.hpp>
#include "rendering/star_renderer.h++"
#include "tools/camera_controller.h++"
#include "tools/camera.h++"
#include "world/starfield.h++"
#include <vector>

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

    // Generate stars
    const StarfieldConfig starfieldConfig;
    const std::vector<Star> stars = createStarfield(starfieldConfig);
    const StarRenderer starRenderer({starfieldConfig.maxDepth, 1.f, 4.f});

    // Main update loop
    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();

        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        updateCameraFromKeyboard(camera, dt);

        if (printClock.getElapsedTime().asSeconds() >= 0.25f)
        {
            Vec3 relative = stars.front().position - camera.position;

            std::cout
                << "Camera: ("
                << camera.position.x << ", "
                << camera.position.y << ", "
                << camera.position.z << ")\n";

            std::cout
                << "First star: ("
                << relative.x << ", "
                << relative.y << ", "
                << relative.z << ")\n";

            printClock.restart();
        }

        window.clear(sf::Color::Black);
        starRenderer.draw(window, stars, camera);
        window.display();
    }

    return 0;
}
