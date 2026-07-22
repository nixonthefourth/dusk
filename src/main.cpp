#include <iostream>
#include <SFML/Graphics.hpp>
#include "camera.h++"

int main() {
    sf::RenderWindow window(
        sf::VideoMode({800, 600}),
        "dusk"
    );

    // Start clock
    sf::Clock clock;
    sf::Clock printClock;

    // Initialise Camera
    Camera camera;
    camera.position = {0.f, 0.f};
    sf::View cam(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));

    // Main update loop
    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();

        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        // WRITE GAMELOOP HERE

        // Move Camera
        // Onwards
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
            camera.position.x += camera.speed * dt;
        }

        // Backwards
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
            camera.position.x -= camera.speed * dt;
        }

        // Left
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
            camera.position.y -= camera.speed * dt;
        }

        // Right
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
            camera.position.y += camera.speed * dt;
        }

        // Output camera positions in the terminal
        if (printClock.getElapsedTime().asSeconds() >= 0.25f)
        {
            std::cout
                << "Camera: ("
                << camera.position.x
                << ", "
                << camera.position.y
                << ")\n";

            printClock.restart();
        }

        cam.setCenter(camera.position);
        window.setView(cam);

        // Enable VSync in order to sync with monitor's settings
        window.setVerticalSyncEnabled(true);

        window.clear(sf::Color::Black);

        window.display();
    }

    return 0;
}
