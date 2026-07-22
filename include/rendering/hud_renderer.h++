//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_HUD_RENDERER_H
#define DUSK_HUD_RENDERER_H

#include "objects/ship.h++"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

/** Minimal ship HUD drawn with primitive rectangles and lines. */
class HudRenderer {
public:
    /** Draws thrust percentage and thrust direction in the bottom-left corner. */
    void draw(sf::RenderTarget& target, const Ship& ship) const
    {
        const sf::Vector2u size = target.getSize();
        const float x = 18.f;
        const float y = static_cast<float>(size.y) - 68.f;
        const float thrust = std::clamp(ship.throttle, 0.f, 1.f);
        const sf::Color accent = ship.reverseThrust
            ? sf::Color(240, 90, 90)
            : sf::Color(110, 220, 255);

        drawRect(target, {x, y}, {118.f, 38.f}, sf::Color(8, 12, 18, 180));
        drawRect(target, {x + 12.f, y + 24.f}, {72.f, 5.f}, sf::Color(60, 70, 78));
        drawRect(target, {x + 12.f, y + 24.f}, {72.f * thrust, 5.f}, accent);
        drawRect(target, {x + 91.f, y + 23.f}, {8.f, 8.f}, accent);

        const int percent = static_cast<int>(std::round(thrust * 100.f));
        drawNumber(target, percent, {x + 12.f, y + 6.f}, accent);
        drawPercentSign(target, {x + 67.f, y + 8.f}, accent);
    }

private:
    static constexpr std::array<unsigned char, 10> digitMasks =
    {
        0x3F, 0x06, 0x5B, 0x4F, 0x66,
        0x6D, 0x7D, 0x07, 0x7F, 0x6F
    };

    static void drawRect(
        sf::RenderTarget& target,
        sf::Vector2f position,
        sf::Vector2f size,
        sf::Color color
    )
    {
        sf::RectangleShape rectangle(size);
        rectangle.setPosition(position);
        rectangle.setFillColor(color);
        target.draw(rectangle);
    }

    static void drawLine(
        sf::RenderTarget& target,
        sf::Vector2f start,
        sf::Vector2f end,
        sf::Color color
    )
    {
        sf::Vertex line[] =
        {
            sf::Vertex(start, color),
            sf::Vertex(end, color)
        };

        target.draw(line, 2, sf::PrimitiveType::Lines);
    }

    static void drawDigit(
        sf::RenderTarget& target,
        int digit,
        sf::Vector2f position,
        sf::Color color
    )
    {
        constexpr float w = 15.f;
        constexpr float h = 24.f;
        constexpr float t = 3.f;
        const unsigned char mask = digitMasks[static_cast<std::size_t>(digit)];

        if (mask & (1 << 0))
            drawRect(target, {position.x + t, position.y}, {w - t * 2.f, t}, color);

        if (mask & (1 << 1))
            drawRect(target, {position.x + w - t, position.y + t}, {t, h * 0.5f - t}, color);

        if (mask & (1 << 2))
            drawRect(target, {position.x + w - t, position.y + h * 0.5f}, {t, h * 0.5f - t}, color);

        if (mask & (1 << 3))
            drawRect(target, {position.x + t, position.y + h - t}, {w - t * 2.f, t}, color);

        if (mask & (1 << 4))
            drawRect(target, {position.x, position.y + h * 0.5f}, {t, h * 0.5f - t}, color);

        if (mask & (1 << 5))
            drawRect(target, {position.x, position.y + t}, {t, h * 0.5f - t}, color);

        if (mask & (1 << 6))
            drawRect(target, {position.x + t, position.y + h * 0.5f - t * 0.5f}, {w - t * 2.f, t}, color);
    }

    static void drawNumber(
        sf::RenderTarget& target,
        int value,
        sf::Vector2f position,
        sf::Color color
    )
    {
        value = std::clamp(value, 0, 100);
        std::vector<int> digits;

        if (value == 100)
        {
            digits = {1, 0, 0};
        }
        else if (value >= 10)
        {
            digits = {value / 10, value % 10};
        }
        else
        {
            digits = {value};
        }

        for (std::size_t i = 0; i < digits.size(); ++i)
            drawDigit(target, digits[i], {position.x + static_cast<float>(i) * 18.f, position.y}, color);
    }

    static void drawPercentSign(sf::RenderTarget& target, sf::Vector2f position, sf::Color color)
    {
        drawRect(target, {position.x, position.y}, {4.f, 4.f}, color);
        drawRect(target, {position.x + 10.f, position.y + 14.f}, {4.f, 4.f}, color);
        drawLine(target, {position.x + 13.f, position.y}, {position.x + 1.f, position.y + 20.f}, color);
    }
};

#endif //DUSK_HUD_RENDERER_H
