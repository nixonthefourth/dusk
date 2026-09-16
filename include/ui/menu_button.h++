//
// Created by Mykyta Khomiakov on 16/09/2026.
//

#ifndef DUSK_MENU_BUTTON_H
#define DUSK_MENU_BUTTON_H

#include <SFML/Graphics.hpp>

/** Small shared helpers for the black/white box buttons used by the menus. */
namespace ui {

/** Bounds of button `index` in a vertically stacked, horizontally centred column. */
inline sf::FloatRect stackedButtonBounds(
    sf::Vector2u targetSize,
    int index,
    int count,
    float yOffset = 0.f,
    float width = 220.f,
    float height = 58.f,
    float gap = 18.f
)
{
    const float totalHeight = height * static_cast<float>(count) + gap * static_cast<float>(count - 1);
    const float x = static_cast<float>(targetSize.x) * 0.5f - width * 0.5f;
    const float y =
        static_cast<float>(targetSize.y) * 0.5f - totalHeight * 0.5f + yOffset +
        static_cast<float>(index) * (height + gap);
    return {{x, y}, {width, height}};
}

/** Centres a text's local bounds on a screen point. */
inline void centerText(sf::Text& text, sf::Vector2f center)
{
    const sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin(
    {
        bounds.position.x + bounds.size.x * 0.5f,
        bounds.position.y + bounds.size.y * 0.5f
    });
    text.setPosition(center);
}

/** Draws a filled (primary) or outlined (secondary) button with a centred label. */
inline void drawButton(
    sf::RenderTarget& target,
    const sf::FloatRect& bounds,
    sf::Text& label,
    bool primary,
    bool hovered
)
{
    sf::RectangleShape button(bounds.size);
    button.setPosition(bounds.position);
    button.setFillColor(primary ? sf::Color::White : sf::Color::Black);
    button.setOutlineColor(primary ? sf::Color::Black : sf::Color::White);
    button.setOutlineThickness(hovered ? 3.f : 2.f);
    target.draw(button);

    label.setFillColor(primary ? sf::Color::Black : sf::Color::White);
    centerText(label, bounds.getCenter());
    target.draw(label);
}

/** Converts an integer mouse position into float screen coordinates. */
inline sf::Vector2f toVector2f(sf::Vector2i vector)
{
    return {static_cast<float>(vector.x), static_cast<float>(vector.y)};
}

} // namespace ui

#endif //DUSK_MENU_BUTTON_H
