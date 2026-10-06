//
// Created by Mykyta Khomiakov on 22/07/2026.
//
// The flight HUD: dashboard (speed, scanner, attitude, target compass), heading and pitch
// tapes, flight markers, and target brackets. Reads world state only; changes nothing.
//

#ifndef DUSK_HUD_RENDERER_H
#define DUSK_HUD_RENDERER_H

#include "objects/ship.h++"
#include "procgen/asteroid_generation.h++"
#include "rendering/projector.h++"
#include "systems/ship_physics.h++"
#include "tools/camera.h++"
#include "ui/format.h++"
#include "ui/style.h++"
#include "world/world.h++"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>

/**
 * Flight HUD, in the spirit of Elite's console:
 *
 *  - a dashboard along the bottom: speed and throttle on the left, a 3D scanner in the middle,
 *    attitude (heading, pitch, turn rates) and the target compass on the right;
 *  - a heading tape across the top and a pitch tape down the right of the view;
 *  - in-view markers: boresight, prograde/retrograde, and brackets on the target (or an arrow at
 *    the screen edge pointing to it when it is off-screen).
 *
 * Everything reads world/ship/camera state and draws; nothing here changes the simulation.
 */
class HudRenderer {
public:
    /** Height of the bottom dashboard; scenes keep their own overlay text clear of it. */
    static constexpr float dashboardHeight = 122.f;

    /** Scanner range in world units: objects further than this are off the scope. */
    static constexpr float scannerRange = 25000.f;

    HudRenderer()
        : font_("assets/fonts/Jersey15-Regular.ttf")
    {
    }

    /** Draws the whole HUD: in-view markers and tapes first, then the dashboard on top. */
    void draw(sf::RenderTarget& target, const World& world, const Camera& camera) const
    {
        const sf::Vector2u size = target.getSize();
        const Viewport viewport = {static_cast<float>(size.x), static_cast<float>(size.y)};
        const Ship& ship = world.playerShip;

        drawFlightMarkers(target, ship, camera, viewport);
        drawTargetMarker(target, world, camera, viewport);
        drawHeadingTape(target, ship, viewport);
        drawPitchTape(target, ship, viewport);

        drawDashboardBackground(target, viewport);
        drawSpeedBlock(target, ship, viewport);
        drawScanner(target, world, viewport);
        drawAttitudeBlock(target, world, viewport);
        drawFieldWarning(target, world, viewport);
    }

private:
    sf::Font font_;
    Projector projector_;


    static constexpr float pi = 3.14159265358979323846f;
    static constexpr float degrees = 180.f / pi;

    /* ---- Primitives ---------------------------------------------------------------------------- */

    static void drawRect(sf::RenderTarget& target, sf::Vector2f position, sf::Vector2f size, sf::Color color)
    {
        sf::RectangleShape rectangle(size);
        rectangle.setPosition(position);
        rectangle.setFillColor(color);
        target.draw(rectangle);
    }

    /** A single one-pixel screen-space line. */
    static void drawLine(sf::RenderTarget& target, sf::Vector2f a, sf::Vector2f b, sf::Color color)
    {
        const sf::Vertex line[] = {sf::Vertex(a, color), sf::Vertex(b, color)};
        target.draw(line, 2, sf::PrimitiveType::Lines);
    }

    /** An ellipse made by scaling a unit circle; the outline thickness is divided back down so it stays one pixel. */
    static void drawEllipse(
        sf::RenderTarget& target,
        sf::Vector2f center,
        sf::Vector2f radii,
        sf::Color outline,
        sf::Color fill = sf::Color::Transparent
    )
    {
        sf::CircleShape ellipse(1.f, 64);
        ellipse.setOrigin({1.f, 1.f});
        ellipse.setPosition(center);
        ellipse.setScale(radii);
        ellipse.setFillColor(fill);
        ellipse.setOutlineColor(outline);
        ellipse.setOutlineThickness(1.f / std::max(1.f, std::min(radii.x, radii.y)));
        target.draw(ellipse);
    }

    /** Draws text with its top-left at `position`, or aligned by `alignX` (0 left, 0.5 centre, 1 right); returns its width. */
    float drawText(
        sf::RenderTarget& target,
        const std::string& string,
        sf::Vector2f position,
        unsigned size,
        sf::Color color,
        float alignX = 0.f
    ) const
    {
        sf::Text text(font_, string, size);
        text.setFillColor(color);
        const sf::FloatRect bounds = text.getLocalBounds();
        text.setOrigin({bounds.position.x + bounds.size.x * alignX, 0.f});
        text.setPosition(position);
        target.draw(text);

        // Width of what was drawn, so callers can place text after it.
        return bounds.size.x;
    }

    /** Ship-local coordinates of a world point: x right, y up, z forward. */
    static Vec3 toShipLocal(const Ship& ship, const Vec3& world)
    {
        const Vec3 offset = world - ship.position;
        return {dot(offset, shipRight(ship)), dot(offset, shipUp(ship)), dot(offset, shipForward(ship))};
    }

    /** Height of the 3D view above the dashboard, in pixels. */
    static float viewHeight(const Viewport& viewport)
    {
        return viewport.height - dashboardHeight;
    }

    /* ---- In-view markers ---------------------------------------------------------------------- */

    /**
     * Boresight cross where the nose points, and a prograde ring where the ship is actually
     * travelling (a red retrograde cross when moving backwards). When the two sit on top of each
     * other, the ship is moving exactly where it is aimed.
     */
    void drawFlightMarkers(sf::RenderTarget& target, const Ship& ship, const Camera& camera, const Viewport& viewport) const
    {
        // A direction's vanishing point is where a point far along it, from the camera, projects.
        const auto directionOnScreen = [&](const Vec3& direction) -> std::optional<sf::Vector2f>
        {
            const auto projected = projector_.project(camera.position + direction * 1000.f, camera, viewport);

            if (!projected)
                return std::nullopt;

            return sf::Vector2f{projected->position.x, projected->position.y};
        };

        const sf::Color markerColor = style::flightMarker;

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
            drawLine(target, {retrograde->x - 6.f, retrograde->y - 6.f}, {retrograde->x + 6.f, retrograde->y + 6.f}, style::retrogradeMarker);
            drawLine(target, {retrograde->x - 6.f, retrograde->y + 6.f}, {retrograde->x + 6.f, retrograde->y - 6.f}, style::retrogradeMarker);
        }
    }

    /**
     * Target brackets sized to the target's projected size, with name and distance underneath.
     * When the target is behind the camera or off-screen, an arrow on an ellipse inside the view
     * points the way to turn instead.
     */
    void drawTargetMarker(sf::RenderTarget& target, const World& world, const Camera& camera, const Viewport& viewport) const
    {
        const auto position = targetPosition(world);

        if (!position)
            return;

        const Ship& ship = world.playerShip;
        const std::string label = std::string(targetLabel(world)) + "  " + formatWorldDistance(length(*position - ship.position));
        const float height = viewHeight(viewport);
        const Vec3 cameraSpace = transformPoint(projector_.createViewMatrix(camera), *position);
        const auto projected = projector_.project(*position, camera, viewport);

        const bool onScreen =
            projected &&
            projected->position.x > 20.f && projected->position.x < viewport.width - 20.f &&
            projected->position.y > 20.f && projected->position.y < height - 20.f;

        if (onScreen)
        {
            const float focal = (viewport.height * 0.5f) / std::tan(camera.fov * 0.5f / degrees);
            const float halfSize = std::clamp(stationBoundingRadius(world.station) * focal / projected->depth, 12.f, 220.f);
            const sf::Vector2f c = {projected->position.x, projected->position.y};
            const float corner = std::max(5.f, halfSize * 0.35f);

            for (const float sx : {-1.f, 1.f})
            {
                for (const float sy : {-1.f, 1.f})
                {
                    const sf::Vector2f p = {c.x + sx * halfSize, c.y + sy * halfSize};
                    drawLine(target, p, {p.x - sx * corner, p.y}, style::targetMarker);
                    drawLine(target, p, {p.x, p.y - sy * corner}, style::targetMarker);
                }
            }

            drawText(target, label, {c.x, c.y + halfSize + 4.f}, 16, style::targetMarker, 0.5f);
            return;
        }

        // Off-screen: point along the target's direction in camera space.
        sf::Vector2f direction = {cameraSpace.x, -cameraSpace.y};

        if (std::hypot(direction.x, direction.y) < 1e-3f)
            direction = {0.f, 1.f};

        const float directionLength = std::hypot(direction.x, direction.y);
        direction /= directionLength;

        const sf::Vector2f centre = {viewport.width * 0.5f, height * 0.5f};
        const sf::Vector2f radii = {viewport.width * 0.5f - 46.f, height * 0.5f - 40.f};
        const sf::Vector2f tip = {centre.x + direction.x * radii.x, centre.y + direction.y * radii.y};
        const sf::Vector2f side = {-direction.y, direction.x};

        sf::ConvexShape arrow(3);
        arrow.setPoint(0, tip + direction * 10.f);
        arrow.setPoint(1, tip - direction * 6.f + side * 8.f);
        arrow.setPoint(2, tip - direction * 6.f - side * 8.f);
        arrow.setFillColor(style::targetMarker);
        target.draw(arrow);

        drawText(target, label, {tip.x - direction.x * 26.f, tip.y - direction.y * 26.f - 8.f}, 15, style::targetMarker, 0.5f);
    }

    /** Bounding radius of the station's model, used to size the target brackets. */
    static float stationBoundingRadius(const Station& station)
    {
        float radius = 0.f;

        for (const Vec3& vertex : station.model.vertices)
            radius = std::max(radius, length(vertex));

        return radius > 0.f ? radius : station.size * 0.5f;
    }

    /* ---- Attitude tapes ----------------------------------------------------------------------- */

    /** Heading in degrees, 0-360, with 000 along world +z. */
    static float headingDegrees(const Ship& ship)
    {
        float heading = std::fmod(ship.yaw * degrees, 360.f);

        if (heading < 0.f)
            heading += 360.f;

        return heading;
    }

    /** Compass tape across the top of the view: ticks every 5 degrees, labels every 30. */
    void drawHeadingTape(sf::RenderTarget& target, const Ship& ship, const Viewport& viewport) const
    {
        const float width = std::min(320.f, viewport.width * 0.42f);
        const float centreX = viewport.width * 0.5f;
        const float baseline = 34.f;
        const float visibleSpan = 60.f;
        const float pixelsPerDegree = width / visibleSpan;
        const float heading = headingDegrees(ship);

        drawLine(target, {centreX - width * 0.5f, baseline}, {centreX + width * 0.5f, baseline}, style::tapeMinorTick);

        const int first = static_cast<int>(std::floor((heading - visibleSpan * 0.5f) / 5.f)) * 5;

        for (int tick = first; tick <= heading + visibleSpan * 0.5f; tick += 5)
        {
            const float x = centreX + (static_cast<float>(tick) - heading) * pixelsPerDegree;

            if (x < centreX - width * 0.5f || x > centreX + width * 0.5f)
                continue;

            const int wrapped = ((tick % 360) + 360) % 360;
            const bool major = wrapped % 30 == 0;
            drawLine(target, {x, baseline}, {x, baseline - (major ? 9.f : 4.f)}, major ? style::tapeMajorTick : style::tapeMinorTick);

            if (major)
                drawText(target, formatHeading(static_cast<float>(wrapped)), {x, baseline + 2.f}, 14, style::tapeMinorTick, 0.5f);
        }

        // Caret and readout.
        sf::ConvexShape caret(3);
        caret.setPoint(0, {centreX, baseline - 1.f});
        caret.setPoint(1, {centreX - 5.f, baseline - 9.f});
        caret.setPoint(2, {centreX + 5.f, baseline - 9.f});
        caret.setFillColor(style::tapeCaret);
        target.draw(caret);

        drawRect(target, {centreX - 22.f, 2.f}, {44.f, 20.f}, style::readoutBoxFill);
        drawText(target, formatHeading(heading), {centreX, 1.f}, 18, style::tapeValue, 0.5f);
    }

    /** Pitch tape down the right of the view: ticks every 5 degrees, labels every 10. */
    void drawPitchTape(sf::RenderTarget& target, const Ship& ship, const Viewport& viewport) const
    {
        const float height = std::min(220.f, viewHeight(viewport) * 0.55f);
        const float centreY = viewHeight(viewport) * 0.5f;
        const float x = viewport.width - 52.f;
        const float visibleSpan = 60.f;
        const float pixelsPerDegree = height / visibleSpan;
        const float pitch = ship.pitch * degrees;

        drawLine(target, {x, centreY - height * 0.5f}, {x, centreY + height * 0.5f}, style::tapeMinorTick);

        const int first = static_cast<int>(std::floor((pitch - visibleSpan * 0.5f) / 5.f)) * 5;

        for (int tick = first; tick <= pitch + visibleSpan * 0.5f; tick += 5)
        {
            const float y = centreY - (static_cast<float>(tick) - pitch) * pixelsPerDegree;

            if (y < centreY - height * 0.5f || y > centreY + height * 0.5f)
                continue;

            const bool major = tick % 10 == 0;
            drawLine(target, {x, y}, {x + (major ? 9.f : 4.f), y}, tick == 0 ? style::tapeCaret : (major ? style::tapeMajorTick : style::tapeMinorTick));

            if (major)
                drawText(target, formatSigned(static_cast<float>(tick)), {x + 12.f, y - 9.f}, 14, tick == 0 ? style::tapeCaret : style::tapeMinorTick);
        }

        sf::ConvexShape caret(3);
        caret.setPoint(0, {x - 1.f, centreY});
        caret.setPoint(1, {x - 9.f, centreY - 5.f});
        caret.setPoint(2, {x - 9.f, centreY + 5.f});
        caret.setFillColor(style::tapeCaret);
        target.draw(caret);

        drawRect(target, {x - 52.f, centreY - 10.f}, {40.f, 20.f}, style::readoutBoxFill);
        drawText(target, formatSigned(pitch), {x - 32.f, centreY - 11.f}, 18, style::tapeValue, 0.5f);
    }

    /* ---- Dashboard ---------------------------------------------------------------------------- */

    void drawDashboardBackground(sf::RenderTarget& target, const Viewport& viewport) const
    {
        const float top = viewport.height - dashboardHeight;
        drawRect(target, {0.f, top}, {viewport.width, dashboardHeight}, style::dashboardFill);
        drawLine(target, {0.f, top}, {viewport.width, top}, style::dashboardEdge);
    }

    /** A labelled horizontal bar filled from the left. */
    void drawFillBar(
        sf::RenderTarget& target,
        const std::string& label,
        sf::Vector2f position,
        float width,
        float fraction,
        sf::Color color
    ) const
    {
        drawText(target, label, {position.x, position.y - 8.f}, 15, style::textDim);
        const float barX = position.x + 34.f;
        drawRect(target, {barX, position.y - 2.f}, {width, 6.f}, style::lineFaint);
        drawRect(target, {barX, position.y - 2.f}, {width * std::clamp(fraction, 0.f, 1.f), 6.f}, color);
    }

    /** A labelled centre-zero bar: fills left for negative, right for positive. */
    void drawCentreBar(
        sf::RenderTarget& target,
        const std::string& label,
        sf::Vector2f position,
        float width,
        float value
    ) const
    {
        drawText(target, label, {position.x, position.y - 8.f}, 15, style::textDim);
        const float barX = position.x + 38.f;
        const float middle = barX + width * 0.5f;
        const float fill = std::clamp(value, -1.f, 1.f) * width * 0.5f;

        drawRect(target, {barX, position.y - 2.f}, {width, 6.f}, style::lineFaint);
        drawRect(target, {std::min(middle, middle + fill), position.y - 2.f}, {std::abs(fill), 6.f}, style::rateBar);
        drawLine(target, {middle, position.y - 5.f}, {middle, position.y + 6.f}, style::rateBarCentre);
    }

    /** Left block: speed readout, throttle and speed bars, flight-mode flags. */
    void drawSpeedBlock(sf::RenderTarget& target, const Ship& ship, const Viewport& viewport) const
    {
        const float top = viewport.height - dashboardHeight;
        const float x = 16.f;
        const float barWidth = std::min(150.f, viewport.width * 0.5f - 175.f);
        const sf::Color throttleColor = ship.reverseThrust ? style::throttleReverse : style::throttleForward;

        drawText(target, ship.cruiseEngaged ? "CRUISE" : "SPEED", {x, top + 10.f}, 15, style::textDim);
        drawText(target, std::to_string(static_cast<int>(std::round(shipSpeed(ship)))), {x + 60.f, top + 2.f}, 28, style::textPrimary);

        // Total mass (hull + fuel + cargo): it drops as fuel burns, and the ship gets livelier.
        char mass[24];
        std::snprintf(mass, sizeof(mass), "%.1f t", shipTotalMass(ship));
        drawText(target, mass, {x + 34.f + barWidth, top + 10.f}, 15, style::textDim, 1.f);

        const float speedScale = ship.cruiseEngaged
            ? ship.cruiseMaxSpeed
            : ship.maxSpeed * (ship.reverseThrust ? ship.reverseSpeedFraction : 1.f);

        drawFillBar(target, ship.reverseThrust ? "REV" : "THR", {x, top + 44.f}, barWidth, ship.throttle, throttleColor);
        drawFillBar(target, "SPD", {x, top + 60.f}, barWidth, speedScale > 0.f ? shipSpeed(ship) / speedScale : 0.f, style::speedBar);

        // Fuel gauge, red below a fifth of the tank, with the tonnes left beside it.
        const float fuelFraction = ship.fuelCapacity > 0.f ? ship.fuel / ship.fuelCapacity : 0.f;
        drawFillBar(target, "FUEL", {x, top + 76.f}, barWidth, fuelFraction, fuelFraction < 0.2f ? style::fuelLow : style::fuelBar);

        char fuel[16];
        std::snprintf(fuel, sizeof(fuel), "%.1f", ship.fuel);
        drawText(target, fuel, {x + 40.f + barWidth, top + 68.f}, 14, fuelFraction < 0.2f ? style::fuelLow : style::textDim);

        // Cargo gauge: tonnes in the hold against the hold's size (the bay module sets the size).
        const float cargoFraction = ship.cargoCapacity > 0.f ? ship.cargoMass / ship.cargoCapacity : 0.f;
        drawFillBar(target, "HOLD", {x, top + 92.f}, barWidth, cargoFraction, style::cargoBar);

        char cargo[24];
        std::snprintf(cargo, sizeof(cargo), "%.0f/%.0f", static_cast<double>(ship.cargoMass), static_cast<double>(ship.cargoCapacity));
        drawText(target, cargo, {x + 40.f + barWidth, top + 84.f}, 14, style::textDim);

        // Flight-assist state gets its own colour (blue when on, orange when off); the cruise
        // status that follows it is drawn separately in the accent.
        const std::string assist = ship.flightAssist ? "FA ON" : "FA OFF";
        std::string cruise;

        if (ship.cruiseEngaged)
            cruise = "[J] DROP";
        else if (cruiseCharging(ship))
            cruise = "CRUISE CHARGING " + std::to_string(static_cast<int>(ship.cruiseCharge * 100.f)) + "%";
        else if (!hasCruiseFuel(ship))
            cruise = "NO FUEL";
        else if (shipMassLocked(ship))
            cruise = "MASS LOCKED";
        else
            cruise = "[J] CRUISE";

        const float assistWidth = drawText(target, assist, {x, top + 101.f}, 17, ship.flightAssist ? style::assistOn : style::assistOff);
        drawText(target, cruise, {x + assistWidth + 16.f, top + 101.f}, 17, hasCruiseFuel(ship) ? style::accent : style::warning);
    }

    /**
     * Elite-style 3D scanner. The ellipse is the ship's horizontal plane seen from above and
     * behind: forward is up the scope, right is right. Each contact sits on the plane at its
     * horizontal position, with a stalk up (or down) to its height above (or below) the ship.
     */
    void drawScanner(sf::RenderTarget& target, const World& world, const Viewport& viewport) const
    {
        const float top = viewport.height - dashboardHeight;
        const sf::Vector2f centre = {viewport.width * 0.5f, top + dashboardHeight * 0.52f};
        const float a = std::min(120.f, viewport.width * 0.15f);
        const sf::Vector2f radii = {a, a * 0.36f};

        drawEllipse(target, centre, radii, style::scopeOutline, style::scopeFill);
        drawEllipse(target, centre, radii * 0.5f, style::scopeGrid);
        drawLine(target, {centre.x - radii.x, centre.y}, {centre.x + radii.x, centre.y}, style::scopeGrid);
        drawLine(target, {centre.x, centre.y - radii.y}, {centre.x, centre.y + radii.y}, style::scopeGrid);

        // Field-of-view wedge.
        const float wedge = 0.78f;
        drawLine(target, centre, {centre.x - radii.x * std::sin(wedge), centre.y - radii.y * std::cos(wedge)}, style::scopeWedge);
        drawLine(target, centre, {centre.x + radii.x * std::sin(wedge), centre.y - radii.y * std::cos(wedge)}, style::scopeWedge);

        drawText(target, formatWorldDistance(scannerRange), {centre.x + radii.x + 4.f, centre.y - 9.f}, 13, style::textDim);

        const Ship& ship = world.playerShip;

        const auto plot = [&](const Vec3& worldPosition, sf::Color color, bool station, bool targeted)
        {
            const Vec3 local = toShipLocal(ship, worldPosition);

            if (std::hypot(local.x, local.z) > scannerRange || std::abs(local.y) > scannerRange)
                return;

            const sf::Vector2f base = {centre.x + local.x / scannerRange * radii.x, centre.y - local.z / scannerRange * radii.y};
            const sf::Vector2f tip = {base.x, base.y - local.y / scannerRange * radii.y * 1.6f};

            drawLine(target, base, tip, color);

            if (station)
            {
                sf::RectangleShape marker({7.f, 7.f});
                marker.setOrigin({3.5f, 3.5f});
                marker.setPosition(tip);
                marker.setFillColor(sf::Color::Transparent);
                marker.setOutlineColor(color);
                marker.setOutlineThickness(1.5f);
                target.draw(marker);
            }
            else
            {
                drawRect(target, {tip.x - 3.f, tip.y - 1.f}, {6.f, 3.f}, color);
            }

            if (targeted)
            {
                sf::CircleShape ring(7.f, 4);
                ring.setOrigin({7.f, 7.f});
                ring.setPosition(tip);
                ring.setFillColor(sf::Color::Transparent);
                ring.setOutlineColor(color);
                ring.setOutlineThickness(1.f);
                target.draw(ring);
            }
        };

        // Rocks big enough to matter, as dim specks with faint stalks; drawn first so ships and
        // the station stay on top.
        const sf::Color rockColor = style::scannerRock;
        const sf::Color rockStalk = style::scannerRockStalk;

        const auto plotRock = [&](const Asteroid& rock)
        {
            if (rock.radius < 250.f)
                return;

            const Vec3 local = toShipLocal(ship, rock.position);

            if (std::hypot(local.x, local.z) > scannerRange || std::abs(local.y) > scannerRange)
                return;

            const sf::Vector2f base = {centre.x + local.x / scannerRange * radii.x, centre.y - local.z / scannerRange * radii.y};
            const sf::Vector2f tip = {base.x, base.y - local.y / scannerRange * radii.y * 1.6f};
            drawLine(target, base, tip, rockStalk);

            const float speck = rock.radius > 1400.f ? 3.f : 2.f;
            drawRect(target, {tip.x - speck * 0.5f, tip.y - speck * 0.5f}, {speck, speck}, rockColor);
        };

        for (const AsteroidBelt& belt : world.asteroidBelts)
            procgen::forEachAsteroidNear(belt, ship.position, scannerRange, plotRock);

        for (const Asteroid& rock : world.driftingAsteroids)
            plotRock(rock);

        for (const NpcShip& npc : world.npcShips)
        {
            if (npc.isVisible())
                plot(npc.ship.position, style::scannerShip, false, false);
        }

        if (world.stationActive)
            plot(world.station.position, style::scannerStation, true, world.target.type == TargetType::Station);

        // Own ship at the centre.
        drawRect(target, {centre.x - 2.f, centre.y - 2.f}, {4.f, 4.f}, style::scannerOwnShip);
    }

    /** Caution-coloured warning above the scanner while the ship is inside an asteroid belt. */
    void drawFieldWarning(sf::RenderTarget& target, const World& world, const Viewport& viewport) const
    {
        if (!insideAsteroidBelt(world, world.playerShip.position))
            return;

        drawText(target, "ASTEROID FIELD", {viewport.width * 0.5f, viewport.height - dashboardHeight - 24.f}, 17, style::fieldWarning, 0.5f);
    }

    /** Right block: heading and pitch, turn-rate bars, target compass and target readout. */
    void drawAttitudeBlock(sf::RenderTarget& target, const World& world, const Viewport& viewport) const
    {
        const Ship& ship = world.playerShip;
        const float top = viewport.height - dashboardHeight;
        const float a = std::min(120.f, viewport.width * 0.15f);
        const float x = viewport.width * 0.5f + a + 34.f;
        const float barWidth = std::max(60.f, viewport.width - x - 130.f);

        drawText(target, "HDG", {x, top + 10.f}, 15, style::textDim);
        drawText(target, formatHeading(headingDegrees(ship)), {x + 32.f, top + 6.f}, 21, style::textPrimary);
        drawText(target, "PITCH", {x + 82.f, top + 10.f}, 15, style::textDim);
        drawText(target, formatSigned(ship.pitch * degrees), {x + 128.f, top + 6.f}, 21, style::textPrimary);

        drawCentreBar(target, "YAW", {x, top + 46.f}, barWidth, ship.yawSpeed > 0.f ? ship.yawRate / ship.yawSpeed : 0.f);
        drawCentreBar(target, "PCH", {x, top + 64.f}, barWidth, ship.pitchSpeed > 0.f ? ship.pitchRate / ship.pitchSpeed : 0.f);

        // Target compass: where the target lies relative to the nose. Filled when ahead, hollow
        // red when behind.
        const sf::Vector2f compass = {viewport.width - 40.f, top + 42.f};
        const float radius = 22.f;

        sf::CircleShape dial(radius, 32);
        dial.setOrigin({radius, radius});
        dial.setPosition(compass);
        dial.setFillColor(style::compassFill);
        dial.setOutlineColor(style::compassOutline);
        dial.setOutlineThickness(1.f);
        target.draw(dial);
        drawLine(target, {compass.x - radius, compass.y}, {compass.x + radius, compass.y}, style::scopeGrid);
        drawLine(target, {compass.x, compass.y - radius}, {compass.x, compass.y + radius}, style::scopeGrid);

        const auto position = targetPosition(world);

        if (!position)
        {
            drawText(target, "NO TARGET   [T] LOCK", {x, top + 88.f}, 17, style::textDim);
            return;
        }

        const Vec3 local = toShipLocal(ship, *position);
        const float distance = length(local);
        const Vec3 direction = distance > 0.f ? local / distance : Vec3{0.f, 0.f, 1.f};
        const bool ahead = direction.z >= 0.f;
        const sf::Vector2f dotPosition = {compass.x + direction.x * radius * 0.85f, compass.y - direction.y * radius * 0.85f};

        sf::CircleShape marker(3.5f, 12);
        marker.setOrigin({3.5f, 3.5f});
        marker.setPosition(dotPosition);
        marker.setFillColor(ahead ? style::compassAhead : sf::Color::Transparent);
        marker.setOutlineColor(ahead ? style::compassAhead : style::compassBehind);
        marker.setOutlineThickness(1.5f);
        target.draw(marker);

        drawText(target, "STN", {compass.x, compass.y + radius + 2.f}, 13, style::textDim, 0.5f);

        // Closing speed: positive while the gap shrinks.
        const Vec3 toTarget = *position - ship.position;
        const float gap = length(toTarget);
        const float closing = gap > 0.f ? dot(ship.velocity - targetVelocity(world), toTarget / gap) : 0.f;

        drawText(
            target,
            std::string(targetLabel(world)) + "  " + formatWorldDistance(gap) + "   CLOSING " + formatSigned(closing),
            {x, top + 88.f},
            17,
            style::targetMarker
        );
    }
};

#endif //DUSK_HUD_RENDERER_H
