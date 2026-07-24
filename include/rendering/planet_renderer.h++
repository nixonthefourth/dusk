//
// Created by Mykyta Khomiakov on 24/07/2026.
//

#ifndef DUSK_PLANET_RENDERER_H
#define DUSK_PLANET_RENDERER_H

#include "math/Mat4.h++"
#include "objects/planet.h++"
#include "rendering/projector.h++"
#include "tools/camera.h++"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

/** Draws projected spherical planets as ascetic monochrome wire models. */
class PlanetRenderer {
public:
    /** Creates a planet renderer using shared projection clipping settings. */
    explicit PlanetRenderer(ProjectionConfig projectionConfig = {})
        : projector_(projectionConfig)
    {
    }

    /** Projects, sorts, and draws planets from back to front. */
    void draw(sf::RenderTarget& target, const std::vector<Planet>& planets, const Camera& camera) const
    {
        const sf::Vector2u size = target.getSize();
        const Viewport viewport =
        {
            static_cast<float>(size.x),
            static_cast<float>(size.y)
        };

        const Mat4 viewMatrix = projector_.createViewMatrix(camera);
        const float focalLength = focalLengthFor(camera, viewport);
        std::vector<ProjectedPlanet> visiblePlanets;
        visiblePlanets.reserve(planets.size());

        for (const Planet& planet : planets)
        {
            const Vec3 cameraSpace = transformPoint(viewMatrix, planet.position);

            if (cameraSpace.z <= 1.f)
                continue;

            const auto projected = projector_.projectCameraSpace(cameraSpace, camera, viewport);

            if (!projected)
                continue;

            const float screenRadius = planet.radius * focalLength / cameraSpace.z;

            if (screenRadius < 2.f)
                continue;

            if (projected->position.x < -screenRadius ||
                projected->position.x > viewport.width + screenRadius ||
                projected->position.y < -screenRadius ||
                projected->position.y > viewport.height + screenRadius)
            {
                continue;
            }

            visiblePlanets.push_back({&planet, *projected, screenRadius});
        }

        std::sort(
            visiblePlanets.begin(),
            visiblePlanets.end(),
            [](const ProjectedPlanet& a, const ProjectedPlanet& b) {
                return a.projected.depth > b.projected.depth;
            }
        );

        for (const ProjectedPlanet& projectedPlanet : visiblePlanets)
            drawPlanet(target, projectedPlanet, camera, viewport, viewMatrix);
    }

private:
    struct ProjectedPlanet {
        const Planet* planet = nullptr;
        ProjectedPoint projected;
        float screenRadius = 0.f;
    };

    Projector projector_;

    static constexpr float pi = 3.14159265358979323846f;

    static float degreesToRadians(float degrees)
    {
        return degrees * pi / 180.f;
    }

    static float focalLengthFor(const Camera& camera, const Viewport& viewport)
    {
        return (viewport.height * 0.5f) / std::tan(degreesToRadians(camera.fov) * 0.5f);
    }

    void drawPlanet(
        sf::RenderTarget& target,
        const ProjectedPlanet& projectedPlanet,
        const Camera& camera,
        const Viewport& viewport,
        const Mat4& viewMatrix
    ) const
    {
        const Planet& planet = *projectedPlanet.planet;
        const float radius = projectedPlanet.screenRadius;
        const sf::Vector2f center =
        {
            projectedPlanet.projected.position.x,
            projectedPlanet.projected.position.y
        };

        if (planet.hasRing)
            drawRing(target, planet, center, radius);

        drawSphereGrid(target, planet, camera, viewport, viewMatrix, projectedPlanet.screenRadius);
        drawSilhouette(target, center, radius);
    }

    void drawSphereGrid(
        sf::RenderTarget& target,
        const Planet& planet,
        const Camera& camera,
        const Viewport& viewport,
        const Mat4& viewMatrix,
        float screenRadius
    ) const
    {
        const int segments = screenRadius > 80.f ? 72 : 48;
        const int latitudeBands = screenRadius > 110.f ? 8 : 6;
        const int meridianBands = screenRadius > 110.f ? 10 : 8;

        for (int latitudeIndex = 1; latitudeIndex < latitudeBands; ++latitudeIndex)
        {
            const float latitude =
                -pi * 0.5f + static_cast<float>(latitudeIndex) * pi / static_cast<float>(latitudeBands);
            drawLatitudeRing(target, planet, camera, viewport, viewMatrix, latitude, segments);
        }

        for (int meridianIndex = 0; meridianIndex < meridianBands; ++meridianIndex)
        {
            const float longitude =
                static_cast<float>(meridianIndex) * pi / static_cast<float>(meridianBands);
            drawMeridianRing(target, planet, camera, viewport, viewMatrix, longitude, segments);
        }
    }

    void drawLatitudeRing(
        sf::RenderTarget& target,
        const Planet& planet,
        const Camera& camera,
        const Viewport& viewport,
        const Mat4& viewMatrix,
        float latitude,
        int segments
    ) const
    {
        Vec3 previous = latitudePoint(planet, latitude, 0.f);

        for (int segment = 1; segment <= segments; ++segment)
        {
            const float angle = static_cast<float>(segment) * 2.f * pi / static_cast<float>(segments);
            const Vec3 current = latitudePoint(planet, latitude, angle);
            drawWireSegment(target, planet, camera, viewport, viewMatrix, previous, current, 0.86f);
            previous = current;
        }
    }

    void drawMeridianRing(
        sf::RenderTarget& target,
        const Planet& planet,
        const Camera& camera,
        const Viewport& viewport,
        const Mat4& viewMatrix,
        float longitude,
        int segments
    ) const
    {
        Vec3 previous = meridianPoint(planet, longitude, 0.f);

        for (int segment = 1; segment <= segments; ++segment)
        {
            const float angle = static_cast<float>(segment) * 2.f * pi / static_cast<float>(segments);
            const Vec3 current = meridianPoint(planet, longitude, angle);
            drawWireSegment(target, planet, camera, viewport, viewMatrix, previous, current, 1.f);
            previous = current;
        }
    }

    void drawWireSegment(
        sf::RenderTarget& target,
        const Planet& planet,
        const Camera& camera,
        const Viewport& viewport,
        const Mat4& viewMatrix,
        const Vec3& startWorld,
        const Vec3& endWorld,
        float alphaScale
    ) const
    {
        const Vec3 start = transformPoint(viewMatrix, startWorld);
        const Vec3 end = transformPoint(viewMatrix, endWorld);
        const auto clipped = projector_.clipLineCameraSpace(start, end, camera, viewport);

        if (!clipped)
            return;

        const auto projectedStart = projector_.projectCameraSpace(clipped->start, camera, viewport);
        const auto projectedEnd = projector_.projectCameraSpace(clipped->end, camera, viewport);

        if (!projectedStart || !projectedEnd)
            return;

        const sf::Color color = lineColorFor(planet, camera, startWorld, endWorld, alphaScale);
        sf::Vertex vertices[] =
        {
            sf::Vertex({projectedStart->position.x, projectedStart->position.y}, color),
            sf::Vertex({projectedEnd->position.x, projectedEnd->position.y}, color)
        };

        target.draw(vertices, 2, sf::PrimitiveType::Lines);
    }

    static Vec3 latitudePoint(const Planet& planet, float latitude, float angle)
    {
        const float ringRadius = planet.radius * std::cos(latitude);
        return planet.position + Vec3
        {
            ringRadius * std::cos(angle),
            planet.radius * std::sin(latitude),
            ringRadius * std::sin(angle)
        };
    }

    static Vec3 meridianPoint(const Planet& planet, float longitude, float angle)
    {
        const float horizontalRadius = planet.radius * std::cos(angle);
        return planet.position + Vec3
        {
            horizontalRadius * std::cos(longitude),
            planet.radius * std::sin(angle),
            horizontalRadius * std::sin(longitude)
        };
    }

    static sf::Color lineColorFor(
        const Planet& planet,
        const Camera& camera,
        const Vec3& startWorld,
        const Vec3& endWorld,
        float alphaScale
    )
    {
        const Vec3 midpoint = (startWorld + endWorld) * 0.5f;
        const Vec3 normal = normalized(midpoint - planet.position);
        const Vec3 toCamera = normalized(camera.position - midpoint);
        const float facing = dot(normal, toCamera);
        const float visibility = std::clamp((facing + 0.25f) / 1.25f, 0.f, 1.f);
        const float alpha = std::clamp((42.f + visibility * 190.f) * alphaScale, 0.f, 255.f);

        return sf::Color(255, 255, 255, static_cast<std::uint8_t>(alpha));
    }

    static void drawSilhouette(sf::RenderTarget& target, sf::Vector2f center, float radius)
    {
        sf::CircleShape silhouette(radius, 128);
        silhouette.setOrigin({radius, radius});
        silhouette.setPosition(center);
        silhouette.setFillColor(sf::Color::Transparent);
        silhouette.setOutlineColor(sf::Color(255, 255, 255, 240));
        silhouette.setOutlineThickness(std::max(1.f, radius * 0.006f));
        target.draw(silhouette);
    }

    static void drawRing(
        sf::RenderTarget& target,
        const Planet& planet,
        sf::Vector2f center,
        float radius
    )
    {
        sf::CircleShape ring(1.f, 128);
        ring.setOrigin({1.f, 1.f});
        ring.setPosition(center);
        ring.setScale({radius * 1.95f, radius * planet.ringFlattening});
        ring.setRotation(sf::degrees(planet.ringRotationDegrees));
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineColor(sf::Color(255, 255, 255, 150));
        ring.setOutlineThickness(std::max(0.012f, 0.035f / radius));
        target.draw(ring);
    }
};

#endif //DUSK_PLANET_RENDERER_H
