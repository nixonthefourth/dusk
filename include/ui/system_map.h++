//
// System map: a top-down orbital view of the current system, with a list of its bodies.
//

#ifndef DUSK_SYSTEM_MAP_H
#define DUSK_SYSTEM_MAP_H

#include "objects/ship.h++"
#include "ui/format.h++"
#include "ui/menu_button.h++"
#include "ui/style.h++"
#include "world/world.h++"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

/** What the scene should do after the system map has handled an event. */
enum class SystemMapAction { None, Close, OpenGalaxyMap, TargetStation };

/** Roman numeral for a planet's position in its system (1-based), enough for any roll we make. */
inline std::string romanNumeral(int value)
{
    static const std::array<const char*, 12> numerals =
    {
        "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII"
    };

    if (value >= 1 && value <= static_cast<int>(numerals.size()))
        return numerals[static_cast<std::size_t>(value - 1)];

    return std::to_string(value);
}

/** Display name for a planet: the system's name plus its orbital order, e.g. "Jorel Minor II". */
inline std::string planetDisplayName(const std::string& systemName, int planetIndex)
{
    return systemName + " " + romanNumeral(planetIndex + 1);
}

/**
 * Top-down (x/z plane) map of the current system: the star, every planet on its live orbit,
 * the station beside its host, NPC traffic, and the player's position and heading. Planets are
 * listed on the right; up/down or a click highlights one and shows its details.
 *
 * Orbits are to scale; body sizes are not (at true scale every planet would be a single pixel).
 */
class SystemMap {
public:
    static constexpr float panelWidth = 250.f;

    /** Resets the highlight to the first planet each time the map opens. */
    void open()
    {
        highlighted_ = 0;
        hovered_ = -2;
    }

    /**
     * Handles one input event and returns what the scene should do. Up/Down cycle the highlight
     * through the star and planets; clicks on a body or a list row highlight it.
     */
    SystemMapAction handleEvent(const sf::Event& event, const sf::RenderWindow& window, const World& world)
    {
        const sf::Vector2u size = window.getSize();
        const int planetCount = static_cast<int>(world.planets.size());

        if (const auto* key = event.getIf<sf::Event::KeyPressed>())
        {
            switch (key->code)
            {
                case sf::Keyboard::Key::Escape:
                case sf::Keyboard::Key::M:
                    return SystemMapAction::Close;

                case sf::Keyboard::Key::G:
                    return SystemMapAction::OpenGalaxyMap;

                case sf::Keyboard::Key::T:
                    return SystemMapAction::TargetStation;

                // -1 is the star; 0.. are planets.
                case sf::Keyboard::Key::Up:
                case sf::Keyboard::Key::Left:
                    highlighted_ = highlighted_ <= -1 ? planetCount - 1 : highlighted_ - 1;
                    break;

                case sf::Keyboard::Key::Down:
                case sf::Keyboard::Key::Right:
                    highlighted_ = highlighted_ >= planetCount - 1 ? -1 : highlighted_ + 1;
                    break;

                default:
                    break;
            }
        }

        if (const auto* moved = event.getIf<sf::Event::MouseMoved>())
            hovered_ = bodyNear(world, ui::toVector2f(moved->position), size);

        if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
        {
            if (pressed->button == sf::Mouse::Button::Left)
            {
                const int clicked = bodyNear(world, ui::toVector2f(pressed->position), size);

                if (clicked >= -1)
                    highlighted_ = clicked;
            }
        }

        highlighted_ = std::clamp(highlighted_, -1, std::max(-1, planetCount - 1));
        return SystemMapAction::None;
    }

    /** Draws the map full-screen: orbits, belts, star, planets, station, traffic, you, scale bar, panel. */
    void draw(sf::RenderTarget& target, const sf::Font& font, const World& world, const std::string& systemName) const
    {
        const sf::Vector2u size = target.getSize();

        sf::RectangleShape backdrop({static_cast<float>(size.x), static_cast<float>(size.y)});
        backdrop.setFillColor(style::background);
        target.draw(backdrop);

        drawOrbits(target, world, size);
        drawBelts(target, font, world, size);
        drawStar(target, world, size);
        drawPlanets(target, font, world, size);
        drawStation(target, world, size);
        drawTraffic(target, world, size);
        drawPlayerArrow(target, world, size);
        drawScaleBar(target, font, world, size);
        drawPanel(target, font, world, systemName, size);
    }

private:
    /** Highlighted body: -1 is the star, 0.. are planets. */
    int highlighted_ = 0;

    /** Body under the mouse, or -2 for none. */
    int hovered_ = -2;


    /** Width of the map area, left of the info panel. */
    static float mapWidth(sf::Vector2u size)
    {
        return static_cast<float>(size.x) - panelWidth;
    }

    /** Pixels per world unit, fitting the outermost orbit into the map area. */
    static float mapScale(const World& world, sf::Vector2u size)
    {
        float extent = world.star.radius * 2.f;

        for (const AsteroidBelt& belt : world.asteroidBelts)
        {
            if (belt.hostPlanetIndex < 0)
                extent = std::max(extent, belt.centreRadius + belt.halfWidth);
        }

        for (const Planet& planet : world.planets)
            extent = std::max(extent, std::hypot(planet.position.x, planet.position.z) + planet.radius);

        return std::min(mapWidth(size), static_cast<float>(size.y)) * 0.44f / extent;
    }

    /** Top-down projection: world x to screen right, world z to screen up, star at the map centre. */
    static sf::Vector2f toScreen(const Vec3& position, const World& world, sf::Vector2u size)
    {
        const float scale = mapScale(world, size);
        return
        {
            mapWidth(size) * 0.5f + (position.x - world.star.position.x) * scale,
            static_cast<float>(size.y) * 0.5f - (position.z - world.star.position.z) * scale
        };
    }

    /** Drawn radius of a planet: grows with its real radius, but never to scale. */
    static float planetPixelRadius(const Planet& planet)
    {
        return 4.f + 9.f * std::clamp((planet.radius - 10000.f) / 24000.f, 0.f, 1.f);
    }

    static constexpr float starPixelRadius = 10.f;

    /** Body under a screen point: -1 star, 0.. planets, -2 none. */
    static int bodyNear(const World& world, sf::Vector2f point, sf::Vector2u size)
    {
        if (point.x > mapWidth(size))
            return listRowAt(world, point, size);

        const sf::Vector2f star = toScreen(world.star.position, world, size);

        for (std::size_t index = 0; index < world.planets.size(); ++index)
        {
            const sf::Vector2f screen = toScreen(world.planets[index].position, world, size);

            if (std::hypot(screen.x - point.x, screen.y - point.y) < planetPixelRadius(world.planets[index]) + 8.f)
                return static_cast<int>(index);
        }

        if (std::hypot(star.x - point.x, star.y - point.y) < starPixelRadius + 8.f)
            return -1;

        return -2;
    }

    static constexpr float listTop = 92.f;
    static constexpr float listRowHeight = 24.f;

    /** Which row of the body list is under a point in the panel. */
    static int listRowAt(const World& world, sf::Vector2f point, sf::Vector2u)
    {
        const int row = static_cast<int>(std::floor((point.y - listTop) / listRowHeight));

        if (row < 0 || row > static_cast<int>(world.planets.size()))
            return -2;

        return row - 1; // row 0 is the star
    }

    /** A single one-pixel line. */
    static void drawLine(sf::RenderTarget& target, sf::Vector2f a, sf::Vector2f b, sf::Color color)
    {
        const sf::Vertex line[] = {sf::Vertex(a, color), sf::Vertex(b, color)};
        target.draw(line, 2, sf::PrimitiveType::Lines);
    }

    /** A circle with separate outline and fill colours (the fill defaults to transparent). */
    static void drawCircle(
        sf::RenderTarget& target,
        sf::Vector2f center,
        float radius,
        sf::Color outline,
        sf::Color fill = sf::Color::Transparent,
        float thickness = 1.f,
        std::size_t points = 48
    )
    {
        sf::CircleShape circle(radius, points);
        circle.setOrigin({radius, radius});
        circle.setPosition(center);
        circle.setFillColor(fill);
        circle.setOutlineColor(outline);
        circle.setOutlineThickness(thickness);
        target.draw(circle);
    }

    /** Text with its top-left corner at `position`. */
    static void drawLabel(
        sf::RenderTarget& target,
        const sf::Font& font,
        const std::string& string,
        sf::Vector2f position,
        unsigned size,
        sf::Color color
    )
    {
        sf::Text text(font, string, size);
        text.setFillColor(color);
        text.setPosition(position);
        target.draw(text);
    }

    /** Each planet's current orbital radius as a circle around the star, the highlighted one in the accent. */
    void drawOrbits(sf::RenderTarget& target, const World& world, sf::Vector2u size) const
    {
        const sf::Vector2f star = toScreen(world.star.position, world, size);
        const float scale = mapScale(world, size);

        for (std::size_t index = 0; index < world.planets.size(); ++index)
        {
            const Planet& planet = world.planets[index];
            const float orbit = std::hypot(planet.position.x - world.star.position.x, planet.position.z - world.star.position.z) * scale;
            const bool lit = static_cast<int>(index) == highlighted_;
            drawCircle(target, star, orbit, lit ? style::mapOrbitHighlight : style::mapOrbit, sf::Color::Transparent, 1.f, 128);
        }
    }

    /** Each belt as a speckled band between two dotted edge rings, labelled at the top. */
    static void drawBelts(sf::RenderTarget& target, const sf::Font& font, const World& world, sf::Vector2u size)
    {
        const sf::Vector2f star = toScreen(world.star.position, world, size);
        const float scale = mapScale(world, size);
        const sf::Color edge = style::mapBeltEdge;
        const sf::Color speck = style::mapBeltSpeck;

        for (const AsteroidBelt& belt : world.asteroidBelts)
        {
            // A planet's debris belt is far too small to show at map scale: a dotted halo round the planet stands in.
            if (belt.hostPlanetIndex >= 0 && static_cast<std::size_t>(belt.hostPlanetIndex) < world.planets.size())
            {
                const Planet& host = world.planets[static_cast<std::size_t>(belt.hostPlanetIndex)];
                const sf::Vector2f hostScreen = toScreen(host.position, world, size);
                const float halo = planetPixelRadius(host) + 5.f;
                const sf::Color haloColor = style::mapBeltHalo;
                std::vector<sf::Vertex> haloPoints;

                for (const float ring : {halo, halo + 2.5f})
                {
                    for (int i = 0; i < 40; ++i)
                    {
                        const float angle = (static_cast<float>(i) + (ring > halo ? 0.5f : 0.f)) / 40.f * 6.2831853f + belt.rotation;
                        haloPoints.push_back(sf::Vertex({hostScreen.x + std::sin(angle) * ring, hostScreen.y - std::cos(angle) * ring}, haloColor));
                    }
                }

                target.draw(haloPoints.data(), haloPoints.size(), sf::PrimitiveType::Points);
                continue;
            }

            std::vector<sf::Vertex> points;

            // Dotted inner and outer edges.
            for (const float radius : {belt.centreRadius - belt.halfWidth, belt.centreRadius + belt.halfWidth})
            {
                const int dots = std::max(24, static_cast<int>(radius * scale * 0.9f));

                for (int i = 0; i < dots; i += 2)
                {
                    const float angle = static_cast<float>(i) / static_cast<float>(dots) * 6.2831853f;
                    points.push_back(sf::Vertex({star.x + std::sin(angle) * radius * scale, star.y - std::cos(angle) * radius * scale}, edge));
                }
            }

            // A sample of the belt's own dust, flattened onto the map.
            for (std::size_t i = 0; i < belt.dust.size(); i += 2)
            {
                const Vec3 mote = beltToWorld(belt, belt.dust[i]) - world.star.position;
                points.push_back(sf::Vertex({star.x + mote.x * scale, star.y - mote.z * scale}, speck));
            }

            target.draw(points.data(), points.size(), sf::PrimitiveType::Points);

            sf::Text label(font, "BELT", 14);
            label.setFillColor(edge);
            const sf::FloatRect bounds = label.getLocalBounds();
            label.setOrigin({bounds.position.x + bounds.size.x * 0.5f, bounds.position.y + bounds.size.y});
            label.setPosition({star.x, star.y - (belt.centreRadius + belt.halfWidth) * scale - 3.f});
            target.draw(label);
        }
    }

    /** The star as a filled disc, ringed when highlighted. */
    void drawStar(sf::RenderTarget& target, const World& world, sf::Vector2u size) const
    {
        const sf::Vector2f star = toScreen(world.star.position, world, size);
        drawCircle(target, star, starPixelRadius, style::mapStar, style::mapStar);

        if (highlighted_ == -1 || hovered_ == -1)
            drawCircle(target, star, starPixelRadius + 5.f, style::mapHighlight);
    }

    /** Each planet as a small globe icon (size hints at its real size), with its ring and numeral. */
    void drawPlanets(sf::RenderTarget& target, const sf::Font& font, const World& world, sf::Vector2u size) const
    {
        for (std::size_t index = 0; index < world.planets.size(); ++index)
        {
            const Planet& planet = world.planets[index];
            const sf::Vector2f screen = toScreen(planet.position, world, size);
            const float radius = planetPixelRadius(planet);
            const bool lit = static_cast<int>(index) == highlighted_ || static_cast<int>(index) == hovered_;

            drawCircle(target, screen, radius, lit ? style::mapHighlight : style::mapPlanet, style::mapPlanetFill, 1.5f);

            // A hint of the wireframe globe: one meridian, one equator.
            drawLine(target, {screen.x - radius, screen.y}, {screen.x + radius, screen.y}, style::mapPlanetCross);
            drawLine(target, {screen.x, screen.y - radius}, {screen.x, screen.y + radius}, style::mapPlanetCross);

            if (planet.hasRing)
            {
                sf::CircleShape ring(1.f, 40);
                ring.setOrigin({1.f, 1.f});
                ring.setPosition(screen);
                ring.setScale({radius * 1.9f, radius * 0.5f});
                ring.setRotation(sf::degrees(planet.ringRotationDegrees));
                ring.setFillColor(sf::Color::Transparent);
                ring.setOutlineColor(style::mapPlanetRing);
                ring.setOutlineThickness(1.f / radius);
                target.draw(ring);
            }

            if (lit)
                drawCircle(target, screen, radius + 5.f, style::mapHighlight);

            drawLabel(target, font, romanNumeral(static_cast<int>(index) + 1), {screen.x + radius + 5.f, screen.y - radius - 14.f}, 16, lit ? style::mapHighlight : style::textDim);
        }
    }

    /** Station is drawn just outside its host (its true orbit would sit inside the host's dot). */
    void drawStation(sf::RenderTarget& target, const World& world, sf::Vector2u size) const
    {
        if (!world.stationActive ||
            world.stationHostPlanetIndex < 0 ||
            static_cast<std::size_t>(world.stationHostPlanetIndex) >= world.planets.size())
        {
            return;
        }

        const Planet& host = world.planets[static_cast<std::size_t>(world.stationHostPlanetIndex)];
        const sf::Vector2f hostScreen = toScreen(host.position, world, size);
        Vec3 offset = world.station.position - host.position;
        offset.y = 0.f;
        const Vec3 direction = length(offset) > 0.f ? normalized(offset) : Vec3{1.f, 0.f, 0.f};
        const float distance = planetPixelRadius(host) + 8.f;
        const sf::Vector2f screen = {hostScreen.x + direction.x * distance, hostScreen.y - direction.z * distance};

        sf::RectangleShape square({6.f, 6.f});
        square.setOrigin({3.f, 3.f});
        square.setPosition(screen);
        square.setFillColor(style::mapStation);
        target.draw(square);

        if (world.target.type == TargetType::Station)
        {
            drawCircle(target, screen, 8.f, style::mapHighlight, sf::Color::Transparent, 1.f, 4);
            drawCircle(target, screen, 11.f, style::mapHighlight, sf::Color::Transparent, 1.f, 4);
        }
    }

    /** Every visible NPC ship as a small dot. */
    static void drawTraffic(sf::RenderTarget& target, const World& world, sf::Vector2u size)
    {
        sf::CircleShape dot(1.5f, 6);
        dot.setOrigin({1.5f, 1.5f});
        dot.setFillColor(style::mapTraffic);

        for (const NpcShip& npc : world.npcShips)
        {
            if (!npc.isVisible())
                continue;

            dot.setPosition(toScreen(npc.ship.position, world, size));
            target.draw(dot);
        }
    }

    /** Player arrow, pointing along the ship's heading. */
    static void drawPlayerArrow(sf::RenderTarget& target, const World& world, sf::Vector2u size)
    {
        const Ship& ship = world.playerShip;
        const sf::Vector2f screen = toScreen(ship.position, world, size);
        const sf::Vector2f forward = {std::sin(ship.yaw), -std::cos(ship.yaw)};
        const sf::Vector2f side = {-forward.y, forward.x};

        sf::ConvexShape arrow(3);
        arrow.setPoint(0, screen + forward * 10.f);
        arrow.setPoint(1, screen - forward * 6.f + side * 6.f);
        arrow.setPoint(2, screen - forward * 6.f - side * 6.f);
        arrow.setFillColor(style::mapPlayer);
        target.draw(arrow);
    }

    /** A scale bar of a round length (10K, 25K, 50K...) that comes out 60-150 pixels long. */
    static void drawScaleBar(sf::RenderTarget& target, const sf::Font& font, const World& world, sf::Vector2u size)
    {
        const float scale = mapScale(world, size);

        // Pick a round length that comes out between 60 and 150 pixels.
        float length = 10000.f;

        for (float candidate : {10000.f, 25000.f, 50000.f, 100000.f, 250000.f, 500000.f})
        {
            length = candidate;

            if (candidate * scale >= 60.f)
                break;
        }

        const float pixels = length * scale;
        const float x = 20.f;
        const float y = static_cast<float>(size.y) - 26.f;
        const sf::Color color = style::mapScaleBar;

        drawLine(target, {x, y}, {x + pixels, y}, color);
        drawLine(target, {x, y - 4.f}, {x, y + 4.f}, color);
        drawLine(target, {x + pixels, y - 4.f}, {x + pixels, y + 4.f}, color);
        drawLabel(target, font, formatWorldDistance(length), {x + pixels + 8.f, y - 11.f}, 15, color);
        drawLabel(target, font, "BODY SIZES NOT TO SCALE", {x, y - 26.f}, 14, style::textDim);
    }

    /** The right-hand panel: the body list with distances, and the highlighted body's details. */
    void drawPanel(
        sf::RenderTarget& target,
        const sf::Font& font,
        const World& world,
        const std::string& systemName,
        sf::Vector2u size
    ) const
    {
        const float left = static_cast<float>(size.x) - panelWidth;
        const float height = static_cast<float>(size.y);

        sf::RectangleShape panel({panelWidth, height});
        panel.setPosition({left, 0.f});
        panel.setFillColor(style::panelFill);
        panel.setOutlineColor(style::panelOutline);
        panel.setOutlineThickness(1.f);
        target.draw(panel);

        const float x = left + 20.f;
        drawLabel(target, font, "SYSTEM MAP", {x, 16.f}, 18, style::textDim);
        drawLabel(target, font, systemName, {x, 40.f}, 30, style::textPrimary);

        const Ship& ship = world.playerShip;

        // Body list: star first, then planets in orbital order.
        for (int row = -1; row < static_cast<int>(world.planets.size()); ++row)
        {
            const float y = listTop + static_cast<float>(row + 1) * listRowHeight;
            const bool lit = row == highlighted_;

            if (lit)
            {
                sf::RectangleShape bar({panelWidth - 24.f, listRowHeight - 2.f});
                bar.setPosition({left + 12.f, y});
                bar.setFillColor(style::panelRowHighlight);
                target.draw(bar);
            }

            const Planet& body = row < 0 ? world.star : world.planets[static_cast<std::size_t>(row)];
            std::string name = row < 0 ? "STAR" : romanNumeral(row + 1);

            if (row >= 0 && row == world.stationHostPlanetIndex && world.stationActive)
                name += "  + STATION";

            drawLabel(target, font, name, {x, y + 1.f}, 17, lit ? style::mapHighlight : style::textPrimary);
            drawLabel(target, font, formatWorldDistance(length(body.position - ship.position) - body.radius), {left + panelWidth - 78.f, y + 1.f}, 17, style::textDim);
        }

        // Details of the highlighted body.
        float y = listTop + static_cast<float>(world.planets.size() + 1) * listRowHeight + 18.f;
        const Planet& body = highlighted_ < 0 ? world.star : world.planets[static_cast<std::size_t>(highlighted_)];
        const std::string title = highlighted_ < 0 ? systemName + " (STAR)" : planetDisplayName(systemName, highlighted_);

        drawLabel(target, font, title, {x, y}, 19, style::mapHighlight);
        y += 28.f;

        const auto row = [&](const std::string& label, const std::string& value)
        {
            drawLabel(target, font, label, {x, y}, 16, style::textDim);
            drawLabel(target, font, value, {x + 100.f, y}, 16, style::textPrimary);
            y += 21.f;
        };

        row("RADIUS", formatWorldDistance(body.radius));

        if (highlighted_ < 0)
        {
            const auto starBelts = std::count_if(world.asteroidBelts.begin(), world.asteroidBelts.end(), [](const AsteroidBelt& belt)
            {
                return belt.hostPlanetIndex < 0;
            });

            row("BELTS", starBelts == 0 ? "NONE" : std::to_string(starBelts));
        }

        if (highlighted_ >= 0)
        {
            row("ORBIT", formatWorldDistance(length(body.position - world.star.position)));

            char speed[32];
            std::snprintf(speed, sizeof(speed), "%.0f u/s", length(body.velocity));
            row("ORBIT SPEED", speed);
            row("RINGS", body.hasRing ? "YES" : "NO");

            const bool hasDebrisBelt = std::any_of(world.asteroidBelts.begin(), world.asteroidBelts.end(), [&](const AsteroidBelt& belt)
            {
                return belt.hostPlanetIndex == highlighted_;
            });

            row("DEBRIS BELT", hasDebrisBelt ? "YES" : "NO");
            row("STATION", highlighted_ == world.stationHostPlanetIndex && world.stationActive ? "IN ORBIT" : "NONE");
        }

        row("ALTITUDE", formatWorldDistance(std::max(0.f, length(body.position - ship.position) - body.radius)));

        drawLabel(target, font, "UP/DOWN / CLICK  select", {x, height - 58.f}, 15, style::textDim);
        drawLabel(target, font, world.stationActive ? "T  target station" : "NO STATION HERE", {x, height - 40.f}, 15, style::textDim);
        drawLabel(target, font, "G  galaxy   M  close", {x, height - 22.f}, 15, style::textDim);
    }
};

#endif //DUSK_SYSTEM_MAP_H
