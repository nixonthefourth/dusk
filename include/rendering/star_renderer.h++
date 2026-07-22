//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_STAR_RENDERER_H
#define DUSK_STAR_RENDERER_H

#include "objects/star.h++"
#include "rendering/projector.h++"
#include "tools/camera.h++"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cstdint>
#include <vector>

/** Visual controls for drawing projected stars. */
struct StarRenderSettings {
    /** Distance at which stars fade to their minimum opacity. */
    float fadeDepth = 20000.f;

    /** Smallest star radius in pixels. */
    float minRadius = 1.f;

    /** Largest star radius in pixels. */
    float maxRadius = 4.f;
};

/** Draws stars by projecting their 3D positions into the active render target. */
class StarRenderer {
public:
    /** Creates a star renderer using visual settings and projection clipping settings. */
    explicit StarRenderer(StarRenderSettings settings = {}, ProjectionConfig projectionConfig = {})
        : settings_(settings),
          projector_(projectionConfig)
    {
    }

    /** Projects, clips, fades, and draws each star. */
    void draw(sf::RenderTarget& target, const std::vector<Star>& stars, const Camera& camera) const
    {
        const sf::Vector2u size = target.getSize();
        const Viewport viewport =
        {
            static_cast<float>(size.x),
            static_cast<float>(size.y)
        };

        sf::CircleShape shape;
        const Mat4 viewMatrix = projector_.createViewMatrix(camera);

        for (const Star& star : stars)
        {
            const Vec3 cameraSpace = transformPoint(viewMatrix, star.position);
            const auto projected = projector_.projectCameraSpace(cameraSpace, camera, viewport);

            if (!projected)
                continue;

            const float brightness = 1.f - std::clamp(projected->depth / settings_.fadeDepth, 0.f, 1.f);
            const float radius = settings_.minRadius + (settings_.maxRadius - settings_.minRadius) * brightness;

            if (projected->position.x < -radius ||
                projected->position.x > viewport.width + radius ||
                projected->position.y < -radius ||
                projected->position.y > viewport.height + radius)
            {
                continue;
            }

            const auto alpha = static_cast<std::uint8_t>(40.f + 215.f * brightness);

            shape.setRadius(radius);
            shape.setOrigin({radius, radius});
            shape.setPosition({projected->position.x, projected->position.y});
            shape.setFillColor(sf::Color(255, 255, 255, alpha));

            target.draw(shape);
        }
    }

private:
    StarRenderSettings settings_;
    Projector projector_;
};

#endif //DUSK_STAR_RENDERER_H
