//
// Cruise and hyperspace travel animations.
//

#ifndef DUSK_TRAVEL_EFFECTS_RENDERER_H
#define DUSK_TRAVEL_EFFECTS_RENDERER_H

#include "math/Mat4.h++"
#include "rendering/projector.h++"
#include "ui/style.h++"
#include "systems/ship_physics.h++"
#include "systems/travel_effects.h++"
#include "tools/camera.h++"
#include "world/world.h++"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

/**
 * Draws the travel animations over the 3D view (after the world, before the HUD):
 *
 * Cruise
 *  - Charging: lines of energy converge on the nose's vanishing point as the drive spools.
 *  - Engaging: they burst outward with a quick white flash.
 *  - Cruising: every star is motion-blurred into a streak along the direction of travel, longer
 *    the faster you go.
 *  - Dropping out: a soft flash and a ring expanding from the nose.
 *
 * Hyperspace
 *  - Countdown: the same converging lines gather during the last seconds.
 *  - Accelerate: the stars stretch outward from the vanishing point into long streaks, faster and
 *    faster, until the screen whites out (the Star Wars jump to lightspeed).
 *  - Tunnel: an opaque hyperspace tunnel — the concentric rings of Elite (1984)'s hyperspace effect,
 *    expanding out of the centre, with pale blue streaks rushing past and a slow twist.
 *  - Arrive: a flash, then the streaks collapse back into stars around the new system.
 */
class TravelEffectsRenderer {
public:
    /** Uses the same projection as ships and stations (the starfield-sized far plane). */
    explicit TravelEffectsRenderer(ProjectionConfig projection = {})
        : projector_(projection)
    {
    }

    /**
     * Draws whichever effects are active. The hyperspace phases replace everything else; otherwise
     * cruise streaks, charge gathering, the engage burst and the drop flash can all layer together.
     */
    void draw(sf::RenderTarget& target, const World& world, const Camera& camera, const TravelEffects& effects) const
    {
        const sf::Vector2u size = target.getSize();
        const Viewport viewport = {static_cast<float>(size.x), static_cast<float>(size.y)};
        const Ship& ship = world.playerShip;
        const sf::Vector2f vanishing = vanishingPoint(ship, camera, viewport);
        const float reach = std::hypot(viewport.width, viewport.height);

        switch (effects.hyperspacePhase)
        {
            case HyperspacePhase::Tunnel:
                drawTunnel(target, viewport, effects);
                return;

            case HyperspacePhase::Accelerate:
            {
                const float p = effects.phaseProgress;
                drawStarStretch(target, world, camera, viewport, vanishing, 0.04f + 7.f * std::pow(p, 2.2f), 1.f);
                drawFlash(target, viewport, std::pow(std::clamp((p - 0.82f) / 0.18f, 0.f, 1.f), 2.f) * 255.f, style::hyperspaceFlash);
                return;
            }

            case HyperspacePhase::Arrive:
            {
                const float p = effects.phaseProgress;
                drawStarStretch(target, world, camera, viewport, vanishing, 7.f * std::pow(1.f - p, 2.6f), 1.f - p * 0.4f);
                drawFlash(target, viewport, std::pow(std::clamp(1.f - p / 0.3f, 0.f, 1.f), 1.5f) * 255.f, style::hyperspaceFlash);
                return;
            }

            case HyperspacePhase::Countdown:
            case HyperspacePhase::None:
                break;
        }

        drawCruiseStreaks(target, world, camera, viewport);

        if (cruiseCharging(ship))
            drawGathering(target, vanishing, reach, ship.cruiseCharge, effects.phaseTime + ship.cruiseCharge * 3.f);

        // The last 1.5 seconds of a hyperspace countdown gather the same way.
        if (effects.hyperspacePhase == HyperspacePhase::Countdown && effects.phaseProgress > 0.f)
            drawGathering(target, vanishing, reach, effects.phaseProgress, effects.phaseTime);

        if (effects.cruiseEngageBurst > 0.f)
        {
            drawBurst(target, vanishing, reach, effects.cruiseEngageBurst);
            drawFlash(target, viewport, std::pow(effects.cruiseEngageBurst, 3.f) * 150.f, style::engageFlash);
        }

        if (effects.cruiseDropFlash > 0.f)
        {
            drawDropRing(target, vanishing, reach, effects.cruiseDropFlash);
            drawFlash(target, viewport, std::pow(effects.cruiseDropFlash, 2.f) * 110.f, style::dropFlash);
        }
    }

private:
    Projector projector_;

    static constexpr float pi = 3.14159265358979323846f;

    /** Screen point the ship's nose points at; travel effects radiate from here. */
    sf::Vector2f vanishingPoint(const Ship& ship, const Camera& camera, const Viewport& viewport) const
    {
        if (const auto projected = projector_.project(camera.position + shipForward(ship) * 1000.f, camera, viewport))
            return {projected->position.x, projected->position.y};

        return {viewport.width * 0.5f, viewport.height * 0.5f};
    }

    /** A full-screen wash of `color` at `alpha` (0-255); skipped when effectively invisible. */
    static void drawFlash(sf::RenderTarget& target, const Viewport& viewport, float alpha, sf::Color color)
    {
        if (alpha < 1.f)
            return;

        sf::RectangleShape flash({viewport.width, viewport.height});
        color.a = static_cast<std::uint8_t>(std::clamp(alpha, 0.f, 255.f));
        flash.setFillColor(color);
        target.draw(flash);
    }

    /** Adds a line to a batch with a colour at each end (alpha fades along streaks). */
    static void addLine(std::vector<sf::Vertex>& lines, sf::Vector2f a, sf::Vector2f b, sf::Color colorA, sf::Color colorB)
    {
        lines.push_back(sf::Vertex(a, colorA));
        lines.push_back(sf::Vertex(b, colorB));
    }

    /** A stable pseudo-random value in [0, 1) for line `index` (no RNG state needed per frame). */
    static float hash01(int index, int salt)
    {
        std::uint32_t h = static_cast<std::uint32_t>(index) * 0x9E3779B1U + static_cast<std::uint32_t>(salt) * 0x85EBCA77U;
        h ^= h >> 15;
        h *= 0x2C1B3C6DU;
        h ^= h >> 12;
        h *= 0x297A2D39U;
        h ^= h >> 15;
        return static_cast<float>(h & 0xFFFFFFU) / 16777216.f;
    }

    /* ---- Cruise ------------------------------------------------------------------------------- */

    /**
     * Motion blur for cruise: each star becomes a streak from where it is back along the direction
     * of travel. The trail length scales with speed, so nearby stars smear across the view while
     * distant ones barely stretch, which reads as depth as well as speed.
     */
    void drawCruiseStreaks(sf::RenderTarget& target, const World& world, const Camera& camera, const Viewport& viewport) const
    {
        const Ship& ship = world.playerShip;
        const float speed = length(ship.velocity);
        const float intensity = std::clamp((speed - 2500.f) / 12000.f, 0.f, 1.f);

        if (intensity <= 0.f)
            return;

        const Vec3 travel = ship.velocity / speed;
        const float trail = speed * 0.5f * intensity; // world units: up to 15,000 at full cruise
        const Mat4 viewMatrix = projector_.createViewMatrix(camera);
        std::vector<sf::Vertex> lines;
        lines.reserve(world.starfield.stars().size() * 2);

        const auto head = static_cast<std::uint8_t>(80.f + 150.f * intensity);
        const sf::Color headColor = style::withAlpha(style::cruiseStreakHead, static_cast<int>(head));
        const sf::Color tailColor = style::cruiseStreakTail;

        for (const Star& star : world.starfield.stars())
        {
            const Vec3 front = transformPoint(viewMatrix, star.position);
            const Vec3 back = transformPoint(viewMatrix, star.position - travel * trail);
            const auto clipped = projector_.clipLineCameraSpace(front, back, camera, viewport);

            if (!clipped)
                continue;

            const auto a = projector_.projectCameraSpace(clipped->start, camera, viewport);
            const auto b = projector_.projectCameraSpace(clipped->end, camera, viewport);

            if (a && b)
                addLine(lines, {a->position.x, a->position.y}, {b->position.x, b->position.y}, headColor, tailColor);
        }

        if (!lines.empty())
            target.draw(lines.data(), lines.size(), sf::PrimitiveType::Lines);
    }

    /** Charging: lines converge on the vanishing point, closing in and brightening as `charge` rises. */
    static void drawGathering(sf::RenderTarget& target, sf::Vector2f centre, float reach, float charge, float time)
    {
        constexpr int lineCount = 56;
        std::vector<sf::Vertex> lines;
        lines.reserve(lineCount * 2);

        for (int i = 0; i < lineCount; ++i)
        {
            const float angle = hash01(i, 1) * 2.f * pi;
            const float phase = std::fmod(hash01(i, 2) + time * (0.6f + charge * 1.6f), 1.f);
            const float outer = reach * (0.75f - 0.45f * phase);
            const float inner = outer * (0.82f - 0.25f * charge);
            const sf::Vector2f direction = {std::cos(angle), std::sin(angle)};
            const auto alpha = static_cast<std::uint8_t>(std::clamp(charge * 170.f * (0.3f + phase), 0.f, 255.f));

            addLine(lines, centre + direction * outer, centre + direction * inner, style::gatherLineOuter, style::withAlpha(style::gatherLineInner, static_cast<int>(alpha)));
        }

        target.draw(lines.data(), lines.size(), sf::PrimitiveType::Lines);

        // A ring tightening on the nose.
        const float ringRadius = 18.f + 110.f * (1.f - charge);
        sf::CircleShape ring(ringRadius, 48);
        ring.setOrigin({ringRadius, ringRadius});
        ring.setPosition(centre);
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineColor(style::withAlpha(style::gatherRing, 60.f + 150.f * charge));
        ring.setOutlineThickness(1.5f);
        target.draw(ring);
    }

    /** Engage: the gathered lines burst outward from the vanishing point. `burst` runs 1 to 0. */
    static void drawBurst(sf::RenderTarget& target, sf::Vector2f centre, float reach, float burst)
    {
        constexpr int lineCount = 72;
        const float travel = 1.f - burst;
        std::vector<sf::Vertex> lines;
        lines.reserve(lineCount * 2);

        for (int i = 0; i < lineCount; ++i)
        {
            const float angle = hash01(i, 3) * 2.f * pi;
            const float speed = 0.6f + 0.8f * hash01(i, 4);
            const float inner = reach * 0.06f + reach * travel * speed;
            const float outer = inner + reach * 0.22f * burst * speed;
            const sf::Vector2f direction = {std::cos(angle), std::sin(angle)};
            const auto alpha = static_cast<std::uint8_t>(230.f * burst);

            addLine(lines, centre + direction * inner, centre + direction * outer, style::withAlpha(style::burstLineHead, static_cast<int>(alpha)), style::burstLineTail);
        }

        target.draw(lines.data(), lines.size(), sf::PrimitiveType::Lines);
    }

    /** Drop-out: a ring expanding from the vanishing point. `flash` runs 1 to 0. */
    static void drawDropRing(sf::RenderTarget& target, sf::Vector2f centre, float reach, float flash)
    {
        const float radius = reach * 0.6f * (1.f - flash) + 10.f;
        sf::CircleShape ring(radius, 64);
        ring.setOrigin({radius, radius});
        ring.setPosition(centre);
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineColor(style::withAlpha(style::dropRing, 200.f * flash));
        ring.setOutlineThickness(2.f);
        target.draw(ring);
    }

    /* ---- Hyperspace --------------------------------------------------------------------------- */

    /**
     * Stretches every on-screen star into a streak pointing away from the vanishing point: from the
     * star's own position out to `stretch` times further from the centre. Zero stretch leaves plain
     * stars; large values give the jump-to-lightspeed look.
     */
    void drawStarStretch(
        sf::RenderTarget& target,
        const World& world,
        const Camera& camera,
        const Viewport& viewport,
        sf::Vector2f centre,
        float stretch,
        float brightness
    ) const
    {
        std::vector<sf::Vertex> lines;
        lines.reserve(world.starfield.stars().size() * 2);

        const auto alpha = static_cast<std::uint8_t>(std::clamp(255.f * brightness, 0.f, 255.f));
        const sf::Color inner = style::withAlpha(style::stretchInner, static_cast<int>(alpha / 3));
        const sf::Color outer = style::withAlpha(style::stretchOuter, static_cast<int>(alpha));

        for (const Star& star : world.starfield.stars())
        {
            const auto projected = projector_.project(star.position, camera, viewport);

            if (!projected)
                continue;

            const sf::Vector2f position = {projected->position.x, projected->position.y};
            const sf::Vector2f offset = position - centre;
            addLine(lines, position, centre + offset * (1.f + stretch), inner, outer);
        }

        if (!lines.empty())
            target.draw(lines.data(), lines.size(), sf::PrimitiveType::Lines);
    }

    /**
     * The hyperspace tunnel. Rings sit at evenly spaced depths that slide toward the viewer; each is
     * drawn at radius focal * tunnelRadius / depth, so they bloom out of the centre and race past the
     * edges, the effect the original Elite used for hyperspace. Streaks ride the same depth scheme at
     * random angles, and the tunnel's centre drifts in a slow loop so it seems to twist and bank.
     */
    static void drawTunnel(sf::RenderTarget& target, const Viewport& viewport, const TravelEffects& effects)
    {
        const float t = effects.phaseTime;
        const sf::Vector2f screenCentre = {viewport.width * 0.5f, viewport.height * 0.5f};
        const float focal = viewport.height * 0.5f;
        const float reach = std::hypot(viewport.width, viewport.height);

        sf::RectangleShape space({viewport.width, viewport.height});
        space.setFillColor(style::tunnelBackground);
        target.draw(space);

        const auto centreAt = [&](float depth)
        {
            // Far rings drift with the twist; near ones settle on the screen centre.
            const float sway = std::clamp(depth / 6.f, 0.f, 1.f);
            return screenCentre + sf::Vector2f{std::sin(t * 1.3f + depth * 0.7f) * 70.f, std::cos(t * 1.1f + depth * 0.5f) * 45.f} * sway;
        };

        constexpr float maxDepth = 8.f;
        constexpr float ringSpacing = 0.5f;
        constexpr float ringSpeed = 4.2f; // depth units per second
        constexpr float tunnelRadius = 1.f;

        // Streaks: pale blue lines rushing outward.
        constexpr int streakCount = 360;
        std::vector<sf::Vertex> lines;
        lines.reserve(streakCount * 2);

        for (int i = 0; i < streakCount; ++i)
        {
            const float angle = hash01(i, 5) * 2.f * pi;
            const float rate = 0.7f + 0.9f * hash01(i, 6);
            const float depth = maxDepth - std::fmod(hash01(i, 7) * maxDepth + t * ringSpeed * 1.6f * rate, maxDepth) + 0.15f;
            const float length = 0.35f + 0.5f * hash01(i, 8);
            const float radiusNear = std::min(reach, focal * tunnelRadius * (0.55f + 0.4f * hash01(i, 9)) / depth);
            const float radiusFar = std::min(reach, focal * tunnelRadius * (0.55f + 0.4f * hash01(i, 9)) / (depth + length));
            const sf::Vector2f direction = {std::cos(angle), std::sin(angle)};
            const float nearness = std::clamp(1.f - depth / maxDepth, 0.f, 1.f);
            const auto alpha = static_cast<std::uint8_t>(70.f + 185.f * nearness);

            addLine(
                lines,
                centreAt(depth + length) + direction * radiusFar,
                centreAt(depth) + direction * radiusNear,
                style::tunnelStreakTail,
                style::withAlpha(style::tunnelStreakHead, static_cast<int>(alpha))
            );
        }

        target.draw(lines.data(), lines.size(), sf::PrimitiveType::Lines);

        // Elite's rings: white outlines blooming from the centre.
        const float offset = std::fmod(t * ringSpeed, ringSpacing);

        for (float depth = ringSpacing - offset; depth < maxDepth; depth += ringSpacing)
        {
            const float radius = focal * tunnelRadius / std::max(depth, 0.05f);

            if (radius > reach)
                continue;

            const float nearness = std::clamp(1.f - depth / maxDepth, 0.f, 1.f);
            sf::CircleShape ring(radius, 72);
            ring.setOrigin({radius, radius});
            ring.setPosition(centreAt(depth));
            ring.setFillColor(sf::Color::Transparent);
            ring.setOutlineColor(style::withAlpha(style::tunnelRing, 30.f + 200.f * nearness * nearness));
            ring.setOutlineThickness(1.f + 1.5f * nearness);
            target.draw(ring);
        }

        // Fade in from the jump's white-out, and back to white just before arrival.
        const float p = effects.phaseProgress;
        const float whiteIn = std::pow(std::clamp(1.f - p / 0.12f, 0.f, 1.f), 1.5f);
        const float whiteOut = std::pow(std::clamp((p - 0.9f) / 0.1f, 0.f, 1.f), 2.f);
        drawFlash(target, viewport, std::max(whiteIn, whiteOut) * 255.f, style::hyperspaceFlash);
    }
};

#endif //DUSK_TRAVEL_EFFECTS_RENDERER_H
