#include <iostream>
#include <SFML/Graphics.hpp>

int main() {
    sf::RenderWindow window(
        sf::VideoMode({800, 600}),
        "dusk"
    );

    // Start clock
    sf::Clock clock;

    // Main update loop
    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();

        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        // Enable VSync in order to sync with monitor's settings
        window.setVerticalSyncEnabled(true);

        window.clear(sf::Color::Black);

        window.display();
    }

    return 0;
}
