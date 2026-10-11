//
// Draws mining: the two laser beams from the muzzles under the nose to the rock, a flash and
// sparks where they strike it, and the ore fragments tumbling from the rock into the hatch.
//
// The beams are drawn in screen space as two layers, a wide soft glow and a narrow bright core,
// both flickering slightly, and fade in and out with the beam's power. Everything here only reads
// World::mining; the mining logic (systems/mining.h++) decides what is there to draw.
//

#ifndef DUSK_MINING_RENDERER_H
#define DUSK_MINING_RENDERER_H

#include "objects/hardpoints.h++"
#include "rendering/projector.h++"
#include "systems/mining.h++"
#include "tools/camera.h++"
#include "ui/style.h++"
#include "world/world.h++"
#include <SFML/Graphics.hpp>
#include <cmath>
#include <cstdint>
#include <optional>

class MiningRenderer {
public:
    /** Uses the same projection as the rest of the 3D view. */
    explicit MiningRenderer(ProjectionConfig projection = {})
        : projector_(projection)
    {
    }

    /** Draws the beams, the impact, and the fragments in flight. */
    void draw(sf::RenderTarget& target, const World& world, const Camera& camera) const
    {
        const mining::MiningState& state = world.mining;

        if (state.beamPower <= 0.f && state.fragments.empty())
            return;

        const sf::Vector2u size = target.getSize();
        const Viewport viewport{static_cast<float>(size.x), static_cast<float>(size.y)};
        const Mat4 view = projector_.createViewMatrix(camera);
        const float time = static_cast<float>(std::fmod(world.elapsedTime, 10000.0));

        if (state.beamPower > 0.f)
            drawBeams(target, world, camera, viewport, view, time);

        drawFragments(target, world, camera, viewport, view, time);
    }

private:
    Projector projector_;

    /** A world point on screen, if it is in front of the camera. */
    std::optional<sf::Vector2f> toScreen(const Vec3& point, const Camera& camera, const Viewport& viewport, const Mat4& view) const
    {
        const auto projected = projector_.projectCameraSpace(transformPoint(view, point), camera, viewport);

        if (!projected)
            return std::nullopt;

        return sf::Vector2f{projected->position.x, projected->position.y};
    }

    /** A world-space segment clipped to the view and projected, if any of it is visible. */
    std::optional<std::pair<sf::Vector2f, sf::Vector2f>> segmentToScreen(const Vec3& a, const Vec3& b, const Camera& camera, const Viewport& viewport, const Mat4& view) const
    {
        const auto clipped = projector_.clipLineCameraSpace(transformPoint(view, a), transformPoint(view, b), camera, viewport);

        if (!clipped)
            return std::nullopt;

        const auto start = projector_.projectCameraSpace(clipped->start, camera, viewport);
        const auto end = projector_.projectCameraSpace(clipped->end, camera, viewport);

        if (!start || !end)
            return std::nullopt;

        return std::pair{sf::Vector2f{start->position.x, start->position.y}, sf::Vector2f{end->position.x, end->position.y}};
    }

    /** A screen-space line `width` pixels thick, as a quad. */
    static void drawThickLine(sf::RenderTarget& target, sf::Vector2f a, sf::Vector2f b, float width, sf::Color color)
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

    /** A soft filled circle. */
    static void drawGlow(sf::RenderTarget& target, sf::Vector2f centre, float radius, sf::Color color)
    {
        sf::CircleShape glow(radius);
        glow.setOrigin({radius, radius});
        glow.setPosition(centre);
        glow.setFillColor(color);
        target.draw(glow);
    }

    /** A cheap hash of two integers to a number in [0, 1), for spark directions that change every frame. */
    static float hashUnit(int a, int b)
    {
        std::uint32_t h = static_cast<std::uint32_t>(a) * 0x9E3779B1u ^ static_cast<std::uint32_t>(b) * 0x85EBCA77u;
        h ^= h >> 15;
        h *= 0x2C1B3C6Du;
        h ^= h >> 12;
        return static_cast<float>(h >> 8) / 16777216.f;
    }

    /** The colour of a resource's ore. */
    static sf::Color oreColor(MiningResource resource)
    {
        switch (resource)
        {
            case MiningResource::Iron: return style::oreIron;
            case MiningResource::Silicates: return style::oreSilicates;
            case MiningResource::Nickel: return style::oreNickel;
            case MiningResource::WaterIce: return style::oreWaterIce;
            case MiningResource::Cobalt: return style::oreCobalt;
            case MiningResource::Platinum: return style::orePlatinum;
        }

        return style::oreIron;
    }

    /** The two beams, the muzzle flares and, when a rock is hit, the impact flash and sparks. */
    void drawBeams(sf::RenderTarget& target, const World& world, const Camera& camera, const Viewport& viewport, const Mat4& view, float time) const
    {
        const mining::MiningState& state = world.mining;
        const Ship& ship = world.playerShip;
        const auto muzzles = hardpointMuzzles(HardpointKind::MiningLaser);
        const Vec3 forward = shipForward(ship);
        const float power = state.beamPower;

        for (std::size_t i = 0; i < muzzles.size(); ++i)
        {
            const Vec3 start = shipLocalToWorld(ship, muzzles[i]);
            const Vec3 end = state.hitting ? state.beamEnd : start + forward * mining::laserRange;

            // A gentle, fast flicker, out of step between the two beams.
            const float flicker = 0.78f + 0.22f * std::sin(time * 53.f + static_cast<float>(i) * 1.7f);

            if (const auto segment = segmentToScreen(start, end, camera, viewport, view))
            {
                drawThickLine(target, segment->first, segment->second, 7.f, style::withAlpha(style::miningBeamGlow, static_cast<int>(70.f * power * flicker)));
                drawThickLine(target, segment->first, segment->second, 2.f, style::withAlpha(style::miningBeamCore, static_cast<int>(235.f * power * flicker)));
            }

            if (const auto muzzle = toScreen(start, camera, viewport, view))
                drawGlow(target, *muzzle, 3.5f * power, style::withAlpha(style::miningBeamCore, static_cast<int>(200.f * power)));
        }

        if (!state.hitting || power < 0.3f)
            return;

        const auto impact = toScreen(state.beamEnd, camera, viewport, view);

        if (!impact)
            return;

        // A pulsing flash where the beams meet the rock...
        const float pulse = 0.5f + 0.5f * std::sin(time * 31.f);
        drawGlow(target, *impact, (8.f + 4.f * pulse) * power, style::withAlpha(style::miningBeamGlow, static_cast<int>(120.f * power)));
        drawGlow(target, *impact, (3.f + 1.5f * pulse) * power, style::withAlpha(style::miningSpark, static_cast<int>(230.f * power)));

        // ...and sparks thrown off it, new ones every thirtieth of a second.
        const int frame = static_cast<int>(time * 30.f);
        sf::VertexArray sparks(sf::PrimitiveType::Lines);

        for (int spark = 0; spark < 9; ++spark)
        {
            const float angle = hashUnit(frame, spark) * 6.2832f;
            const float reach = 6.f + 14.f * hashUnit(frame, spark + 101);
            const sf::Vector2f direction{std::cos(angle), std::sin(angle)};
            const sf::Color color = style::withAlpha(style::miningSpark, static_cast<int>(220.f * power));
            sparks.append(sf::Vertex{*impact + direction * 3.f, color});
            sparks.append(sf::Vertex{*impact + direction * reach, style::withAlpha(style::miningSpark, 0)});
        }

        target.draw(sparks);
    }

    /** The ore fragments flying in: small tumbling triangles in their resource's colour, with a faint trail. */
    void drawFragments(sf::RenderTarget& target, const World& world, const Camera& camera, const Viewport& viewport, const Mat4& view, float time) const
    {
        for (const mining::MiningFragment& fragment : world.mining.fragments)
        {
            const auto centre = toScreen(fragment.position, camera, viewport, view);

            if (!centre)
                continue;

            const sf::Color color = oreColor(fragment.resource);
            const float angle = fragment.spin + time * 7.f;
            const float size = 4.2f;
            sf::ConvexShape shard(3);

            for (std::size_t corner = 0; corner < 3; ++corner)
            {
                const float a = angle + static_cast<float>(corner) * 2.0944f;
                shard.setPoint(corner, {std::cos(a) * size * (corner == 0 ? 1.4f : 1.f), std::sin(a) * size});
            }

            shard.setPosition(*centre);
            shard.setFillColor(color);
            target.draw(shard);

            // A short trail back toward where it came from.
            if (const auto origin = toScreen(fragment.start + fragment.startVelocity * fragment.age, camera, viewport, view))
            {
                const sf::Vector2f back = *origin - *centre;
                const float length = std::hypot(back.x, back.y);

                if (length > 1.f)
                    drawThickLine(target, *centre, *centre + back / length * std::min(10.f, length), 1.f, style::withAlpha(color, 90));
            }
        }
    }
};

#endif //DUSK_MINING_RENDERER_H
