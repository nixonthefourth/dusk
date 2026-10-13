//
// Draws combat: laser bolts as glowing streaks, the beam laser, rockets with their smoke trails,
// sparks, tumbling debris, explosions (a flash and a spreading shock ring), the flash of a ship's
// shield bubble when it is hit, and the turret on top of the player's hull.
//
// Sizes that belong to the world (smoke puffs, debris, shock rings, shield bubbles) are projected
// with distance, so they shrink as they recede. Everything here only reads World::combat.
//

#ifndef DUSK_COMBAT_RENDERER_H
#define DUSK_COMBAT_RENDERER_H

#include "objects/hardpoints.h++"
#include "rendering/projector.h++"
#include "systems/combat.h++"
#include "tools/camera.h++"
#include "ui/style.h++"
#include "world/world.h++"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <optional>

class CombatRenderer {
public:
    /** Uses the same projection as the rest of the 3D view. */
    explicit CombatRenderer(ProjectionConfig projection = {})
        : projector_(projection)
    {
    }

    /** Draws everything combat has in the world this frame. */
    void draw(sf::RenderTarget& target, const World& world, const Camera& camera) const
    {
        const sf::Vector2u size = target.getSize();
        Frame frame{world, camera, Viewport{static_cast<float>(size.x), static_cast<float>(size.y)}, projector_.createViewMatrix(camera), 0.f,
                    static_cast<float>(std::fmod(world.elapsedTime, 10000.0))};
        frame.focal = (frame.viewport.height * 0.5f) / std::tan(camera.fov * 0.5f * 3.14159265f / 180.f);

        if (world.combat.turretFitted)
            drawTurret(target, frame);

        drawParticles(target, frame);
        drawShieldFlashes(target, frame);
        drawBeam(target, frame);
        drawBolts(target, frame);
        drawRockets(target, frame);
        drawExplosions(target, frame);
    }

private:
    Projector projector_;

    /** What every drawing step needs: the world, the camera, the screen, and the clock. */
    struct Frame {
        const World& world;
        const Camera& camera;
        Viewport viewport;
        Mat4 view;
        float focal;
        float time;
    };

    /** A world point on screen, with its distance from the camera, if it is in front. */
    std::optional<std::pair<sf::Vector2f, float>> toScreen(const Frame& frame, const Vec3& point) const
    {
        const auto projected = projector_.projectCameraSpace(transformPoint(frame.view, point), frame.camera, frame.viewport);

        if (!projected)
            return std::nullopt;

        return std::pair{sf::Vector2f{projected->position.x, projected->position.y}, projected->depth};
    }

    /** A world segment clipped and projected, if any of it is visible. */
    std::optional<std::pair<sf::Vector2f, sf::Vector2f>> segment(const Frame& frame, const Vec3& a, const Vec3& b) const
    {
        const auto clipped = projector_.clipLineCameraSpace(transformPoint(frame.view, a), transformPoint(frame.view, b), frame.camera, frame.viewport);

        if (!clipped)
            return std::nullopt;

        const auto start = projector_.projectCameraSpace(clipped->start, frame.camera, frame.viewport);
        const auto end = projector_.projectCameraSpace(clipped->end, frame.camera, frame.viewport);

        if (!start || !end)
            return std::nullopt;

        return std::pair{sf::Vector2f{start->position.x, start->position.y}, sf::Vector2f{end->position.x, end->position.y}};
    }

    /** A world size seen at `depth`, in pixels, kept within a range so it never vanishes or fills the screen. */
    static float pixels(const Frame& frame, float worldSize, float depth, float lowest, float highest)
    {
        return std::clamp(worldSize * frame.focal / std::max(1.f, depth), lowest, highest);
    }

    /** A screen-space line `width` pixels thick. */
    static void thickLine(sf::RenderTarget& target, sf::Vector2f a, sf::Vector2f b, float width, sf::Color color)
    {
        const sf::Vector2f along = b - a;
        const float length = std::hypot(along.x, along.y);

        if (length < 0.5f)
            return;

        const sf::Vector2f side{-along.y / length * width * 0.5f, along.x / length * width * 0.5f};
        sf::VertexArray quad(sf::PrimitiveType::TriangleStrip, 4);
        quad[0] = sf::Vertex{a + side, color};
        quad[1] = sf::Vertex{a - side, color};
        quad[2] = sf::Vertex{b + side, color};
        quad[3] = sf::Vertex{b - side, color};
        target.draw(quad);
    }

    /** A filled circle. */
    static void disc(sf::RenderTarget& target, sf::Vector2f centre, float radius, sf::Color color)
    {
        sf::CircleShape circle(radius);
        circle.setOrigin({radius, radius});
        circle.setPosition(centre);
        circle.setFillColor(color);
        target.draw(circle);
    }

    /** An outlined circle. */
    static void ring(sf::RenderTarget& target, sf::Vector2f centre, float radius, float thickness, sf::Color color)
    {
        sf::CircleShape circle(radius, 40);
        circle.setOrigin({radius, radius});
        circle.setPosition(centre);
        circle.setFillColor(sf::Color::Transparent);
        circle.setOutlineColor(color);
        circle.setOutlineThickness(thickness);
        target.draw(circle);
    }

    /** Alpha from 0-255 times a fraction. */
    static int alphaOf(float fraction, int peak = 255)
    {
        return static_cast<int>(std::clamp(fraction, 0.f, 1.f) * static_cast<float>(peak));
    }

    /** The turret on top of the hull: a squat base and a twin barrel along its aim. */
    void drawTurret(sf::RenderTarget& target, const Frame& frame) const
    {
        const Ship& ship = frame.world.playerShip;
        const Vec3 mount = combat::muzzlePosition(ship, HardpointKind::TurretLaser, 0);
        const Vec3 right = shipRight(ship);
        const Vec3 forward = shipForward(ship);
        const Vec3 up = shipUp(ship);
        const sf::Color color = style::turretBody;

        // Base: a small square plate with a raised dome point.
        const float half = 34.f;
        const Vec3 corners[4] = {mount + right * half + forward * half, mount - right * half + forward * half, mount - right * half - forward * half, mount + right * half - forward * half};

        for (int i = 0; i < 4; ++i)
        {
            if (const auto s = segment(frame, corners[i] - up * 12.f, corners[(i + 1) % 4] - up * 12.f))
                thickLine(target, s->first, s->second, 1.f, color);

            if (const auto s = segment(frame, corners[i] - up * 12.f, mount + up * 8.f))
                thickLine(target, s->first, s->second, 1.f, style::withAlpha(color, 160));
        }

        // Twin barrels along the aim.
        const Vec3 aim = frame.world.combat.turretAim;
        Vec3 side = cross(aim, up);
        side = length(side) > 1e-3f ? normalized(side) : right;

        for (const float offset : {-9.f, 9.f})
        {
            const Vec3 root = mount + up * 6.f + side * offset;

            if (const auto s = segment(frame, root, root + aim * 95.f))
                thickLine(target, s->first, s->second, 1.6f, color);
        }
    }

    /** Smoke, sparks, debris and muzzle flashes. */
    void drawParticles(sf::RenderTarget& target, const Frame& frame) const
    {
        sf::VertexArray sparks(sf::PrimitiveType::Lines);

        for (const combat::Particle& particle : frame.world.combat.particles)
        {
            const float fade = 1.f - particle.age / particle.life;

            switch (particle.kind)
            {
                case combat::ParticleKind::Smoke:
                {
                    const auto p = toScreen(frame, particle.position);

                    if (p)
                        disc(target, p->first, pixels(frame, 16.f + 40.f * (particle.age / particle.life), p->second, 1.f, 14.f), style::withAlpha(style::rocketSmoke, alphaOf(fade, 110)));

                    break;
                }

                case combat::ParticleKind::Spark:
                {
                    if (const auto s = segment(frame, particle.position, particle.position - particle.velocity * 0.035f))
                    {
                        sparks.append(sf::Vertex{s->first, style::withAlpha(style::sparkHot, alphaOf(fade))});
                        sparks.append(sf::Vertex{s->second, style::withAlpha(style::sparkCool, 0)});
                    }

                    break;
                }

                case combat::ParticleKind::Debris:
                {
                    const auto p = toScreen(frame, particle.position);

                    if (!p)
                        break;

                    const float size = pixels(frame, particle.size, p->second, 1.f, 9.f);
                    const float angle = particle.spin + particle.age * 5.f;
                    const sf::Vector2f arm{std::cos(angle) * size, std::sin(angle) * size};
                    thickLine(target, p->first - arm, p->first + arm, 1.2f, style::withAlpha(style::debris, alphaOf(fade, 230)));
                    break;
                }

                case combat::ParticleKind::Flash:
                {
                    if (const auto p = toScreen(frame, particle.position))
                        disc(target, p->first, pixels(frame, particle.size * 6.f, p->second, 2.f, 10.f) * fade, style::withAlpha(style::muzzleFlash, alphaOf(fade, 230)));

                    break;
                }
            }
        }

        target.draw(sparks);
    }

    /** The shield bubble flaring around a ship where it was hit. */
    void drawShieldFlashes(sf::RenderTarget& target, const Frame& frame) const
    {
        for (const combat::HitFlash& flash : frame.world.combat.flashes)
        {
            const Ship& hitShip = flash.ship == combat::playerShip ? frame.world.playerShip : frame.world.npcShips[static_cast<std::size_t>(flash.ship)].ship;
            const auto centre = toScreen(frame, hitShip.position);

            if (!centre)
                continue;

            const float fade = 1.f - flash.age / 0.35f;
            const float radius = pixels(frame, combat::npcHitRadius, centre->second, 6.f, 260.f);
            ring(target, centre->first, radius, 1.5f, style::withAlpha(style::shieldFlash, alphaOf(fade, 170)));

            if (const auto spot = toScreen(frame, hitShip.position + flash.offset))
                disc(target, spot->first, std::clamp(radius * 0.07f, 2.f, 9.f) * (0.6f + 0.4f * fade), style::withAlpha(style::shieldFlash, alphaOf(fade, 200)));
        }
    }

    /** The beam laser: a glow and a hot core from both muzzles to where it ends, flickering. */
    void drawBeam(sf::RenderTarget& target, const Frame& frame) const
    {
        const combat::CombatState& state = frame.world.combat;

        if (state.beamPower <= 0.f)
            return;

        const Ship& ship = frame.world.playerShip;
        const Vec3 forward = shipForward(ship);

        for (int i = 0; i < 2; ++i)
        {
            const Vec3 start = combat::muzzlePosition(ship, HardpointKind::BeamLaser, i);
            const Vec3 end = state.beamHitting ? state.beamEnd : start + forward * combat::beamRange;
            const float flicker = 0.8f + 0.2f * std::sin(frame.time * 61.f + static_cast<float>(i) * 2.1f);

            if (const auto s = segment(frame, start, end))
            {
                thickLine(target, s->first, s->second, 10.f, style::withAlpha(style::beamGlow, alphaOf(state.beamPower * flicker, 80)));
                thickLine(target, s->first, s->second, 2.6f, style::withAlpha(style::beamCore, alphaOf(state.beamPower * flicker, 245)));
            }
        }

        if (state.beamHitting)
        {
            if (const auto p = toScreen(frame, state.beamEnd))
            {
                const float pulse = 0.5f + 0.5f * std::sin(frame.time * 37.f);
                disc(target, p->first, (9.f + 5.f * pulse) * state.beamPower, style::withAlpha(style::beamGlow, alphaOf(state.beamPower, 140)));
                disc(target, p->first, (3.5f + 2.f * pulse) * state.beamPower, style::withAlpha(style::beamCore, alphaOf(state.beamPower)));
            }
        }
    }

    /** Laser bolts: short glowing streaks along their flight. */
    void drawBolts(sf::RenderTarget& target, const Frame& frame) const
    {
        for (const combat::Bolt& bolt : frame.world.combat.bolts)
        {
            const float speed = length(bolt.velocity - frame.world.playerShip.velocity);
            const Vec3 direction = speed > 1e-3f ? (bolt.velocity - frame.world.playerShip.velocity) / speed : shipForward(frame.world.playerShip);
            const auto s = segment(frame, bolt.position - direction * 90.f, bolt.position);

            if (!s)
                continue;

            const sf::Color glow = bolt.fromTurret ? style::turretBoltGlow : style::pulseBoltGlow;
            const sf::Color core = bolt.fromTurret ? style::turretBoltCore : style::pulseBoltCore;
            thickLine(target, s->first, s->second, 6.f, glow);
            thickLine(target, s->first, s->second, 2.f, core);
        }
    }

    /** Rockets: a hot point with a flickering flame behind it (their smoke is drawn with the particles). */
    void drawRockets(sf::RenderTarget& target, const Frame& frame) const
    {
        for (const combat::Rocket& rocket : frame.world.combat.rockets)
        {
            const float speed = length(rocket.velocity);
            const Vec3 heading = speed > 1e-3f ? rocket.velocity / speed : Vec3{0.f, 0.f, 1.f};
            const float flame = 40.f + 25.f * std::sin(frame.time * 70.f + rocket.life * 13.f);

            if (const auto s = segment(frame, rocket.position - heading * flame, rocket.position))
                thickLine(target, s->first, s->second, 3.f, style::rocketFlame);

            if (const auto s = segment(frame, rocket.position - heading * 14.f, rocket.position + heading * 14.f))
                thickLine(target, s->first, s->second, 2.f, style::rocketBody);

            if (const auto p = toScreen(frame, rocket.position))
                disc(target, p->first, 2.2f, style::rocketFlame);
        }
    }

    /** Explosions: a bright flash that swells and fades, and a shock ring spreading out. */
    void drawExplosions(sf::RenderTarget& target, const Frame& frame) const
    {
        for (const combat::Explosion& explosion : frame.world.combat.explosions)
        {
            const auto p = toScreen(frame, explosion.position);

            if (!p)
                continue;

            const float t = explosion.age / explosion.life;
            const float flashRadius = pixels(frame, explosion.radius * (0.25f + 0.35f * t), p->second, 3.f, 240.f);
            const float ringRadius = pixels(frame, explosion.radius * std::sqrt(t), p->second, 3.f, 400.f);

            disc(target, p->first, flashRadius, style::withAlpha(style::explosionOuter, alphaOf(1.f - t, 150)));
            disc(target, p->first, flashRadius * 0.45f, style::withAlpha(style::explosionCore, alphaOf(1.f - t * 1.4f, 255)));
            ring(target, p->first, ringRadius, 2.f, style::withAlpha(style::explosionRing, alphaOf(1.f - t, 200)));
        }
    }
};

#endif //DUSK_COMBAT_RENDERER_H
