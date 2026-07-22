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
        // z
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
            camera.position.z += camera.speed * dt;
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
            camera.position.z -= camera.speed * dt;
        }

        // x
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
            camera.position.x -= camera.speed * dt;
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
            camera.position.x += camera.speed * dt;
        }

        // y
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q)) {
            camera.position.y += camera.speed * dt;
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E)) {
            camera.position.y -= camera.speed * dt;
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
