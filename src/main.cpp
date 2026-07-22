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

    // Main update loop
    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();

        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        // WRITE GAMELOOP HERE

        // Initialise Camera
        Camera camera;
        camera.position = {0.f, 0.f, 0.f};

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

        // Up
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q)) {
            camera.position.z += camera.speed * dt;
        }

        // Down
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E)) {
            camera.position.z -= camera.speed * dt;
        }

        // Output camera positions in the terminal
        if (printClock.getElapsedTime().asSeconds() >= 0.25f)
        {
            std::cout
                << "Camera: ("
                << camera.position.x
                << ", "
                << camera.position.y
                << ", "
                << camera.position.z
                << ")\n";

            printClock.restart();
        }

        // Enable VSync in order to sync with monitor's settings
        window.setVerticalSyncEnabled(true);

        window.clear(sf::Color::Black);

        window.display();
    }

    return 0;
}
