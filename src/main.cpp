#include <iostream>
#include <SFML/Graphics.hpp>
#include "tools/camera.h++"
#include "objects/star.h++"
#include <random>

int main() {
    sf::RenderWindow window(
        sf::VideoMode({800, 600}),
        "dusk"
    );

    constexpr float SPACE_SIZE = 10000.f;
    constexpr float MIN_DEPTH  = 100.f;
    constexpr float MAX_DEPTH  = 20000.f;

    // Start clock
    sf::Clock clock;
    sf::Clock printClock;

    // Initialise Camera
    Camera camera;

    // Generate stars
    std::vector<Star> stars;
    static std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> xDist(-SPACE_SIZE, SPACE_SIZE);
    std::uniform_real_distribution<float> yDist(-SPACE_SIZE, SPACE_SIZE);
    std::uniform_real_distribution<float> zDist(MIN_DEPTH, MAX_DEPTH);

    constexpr int STAR_COUNT = 1000;

    stars.reserve(STAR_COUNT);

    for (int i = 0; i < STAR_COUNT; ++i)
    {
        stars.push_back(
        {
            {
                xDist(gen),
                yDist(gen),
                zDist(gen)
            }
        });
    }

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
        // z (Forward/Back)
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
            camera.position.z += camera.speed * dt;
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
            camera.position.z -= camera.speed * dt;
        }

        // x (Left/Right)
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
            camera.position.x -= camera.speed * dt;
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
            camera.position.x += camera.speed * dt;
        }

        // y (Up/Down)
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q)) {
            camera.position.y += camera.speed * dt;
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E)) {
            camera.position.y -= camera.speed * dt;
        }

        for (auto& star : stars) {
            Vec3 relative = star.position - camera.position;
        }

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

        // Enable VSync in order to sync with monitor's settings
        window.setVerticalSyncEnabled(true);

        window.clear(sf::Color::Black);

        window.display();
    }

    return 0;
}
