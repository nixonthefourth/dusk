//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_HUD_RENDERER_H
#define DUSK_HUD_RENDERER_H

#include "objects/ship.h++"
#include "rendering/projector.h++"
#include "tools/camera.h++"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
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

        // Actual speed, on the same scale as the throttle demand: with flight assist on, the bar
        // shows where the ship is heading and the tick shows where it has got to.
        const float speedScale = ship.cruiseEngaged
            ? ship.cruiseMaxSpeed
            : ship.maxSpeed * (ship.reverseThrust ? ship.reverseSpeedFraction : 1.f);
        const float speed01 = speedScale > 0.f ? std::clamp(length(ship.velocity) / speedScale, 0.f, 1.f) : 0.f;
        drawRect(target, {x + 12.f + 72.f * speed01 - 1.f, y + 20.f}, {2.f, 13.f}, sf::Color::White);

        const int percent = static_cast<int>(std::round(thrust * 100.f));
        drawNumber(target, percent, {x + 12.f, y + 6.f}, accent);
        drawPercentSign(target, {x + 67.f, y + 8.f}, accent);
    }

    /**
     * Draws two flight markers in world-space directions: a boresight cross where the nose points,
     * and a prograde ring where the ship is actually travelling (or a retrograde cross when moving
     * backwards). Putting the ring on the target is how you fly straight at it; when the two
     * markers sit on top of each other, the ship is moving exactly where it is aimed.
     */
    void drawFlightMarkers(sf::RenderTarget& target, const Ship& ship, const Camera& camera) const
    {
        const sf::Vector2u size = target.getSize();
        const Viewport viewport = {static_cast<float>(size.x), static_cast<float>(size.y)};
        const sf::Color markerColor(110, 220, 255, 220);

        // A direction's vanishing point is where a point far along it, from the camera, projects.
        const auto directionOnScreen = [&](const Vec3& direction) -> std::optional<sf::Vector2f>
        {
            const auto projected = projector_.project(camera.position + direction * 1000.f, camera, viewport);

            if (!projected)
                return std::nullopt;

            return sf::Vector2f{projected->position.x, projected->position.y};
        };

        if (const auto nose = directionOnScreen(shipForward(ship)))
        {
            drawLine(target, {nose->x - 9.f, nose->y}, {nose->x - 3.f, nose->y}, markerColor);
            drawLine(target, {nose->x + 3.f, nose->y}, {nose->x + 9.f, nose->y}, markerColor);
            drawLine(target, {nose->x, nose->y - 9.f}, {nose->x, nose->y - 3.f}, markerColor);
            drawLine(target, {nose->x, nose->y + 3.f}, {nose->x, nose->y + 9.f}, markerColor);
        }

        const float speed = length(ship.velocity);

        if (speed < 5.f)
            return;

        const Vec3 travel = ship.velocity / speed;

        if (const auto prograde = directionOnScreen(travel))
        {
            sf::CircleShape ring(6.f, 20);
            ring.setOrigin({6.f, 6.f});
            ring.setPosition(*prograde);
            ring.setFillColor(sf::Color::Transparent);
            ring.setOutlineColor(markerColor);
            ring.setOutlineThickness(1.5f);
            target.draw(ring);

            drawLine(target, {prograde->x - 13.f, prograde->y}, {prograde->x - 7.f, prograde->y}, markerColor);
            drawLine(target, {prograde->x + 7.f, prograde->y}, {prograde->x + 13.f, prograde->y}, markerColor);
            drawLine(target, {prograde->x, prograde->y - 13.f}, {prograde->x, prograde->y - 7.f}, markerColor);
        }
        else if (const auto retrograde = directionOnScreen(travel * -1.f))
        {
            const sf::Color retroColor(240, 90, 90, 220);
            drawLine(target, {retrograde->x - 6.f, retrograde->y - 6.f}, {retrograde->x + 6.f, retrograde->y + 6.f}, retroColor);
            drawLine(target, {retrograde->x - 6.f, retrograde->y + 6.f}, {retrograde->x + 6.f, retrograde->y - 6.f}, retroColor);
        }
    }

private:
    Projector projector_;

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
