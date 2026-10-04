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

/** Draws projected spherical planets and stars as ascetic monochrome wire models. */
class PlanetRenderer {
public:
    /** Creates a planet renderer using shared projection clipping settings. */
    explicit PlanetRenderer(ProjectionConfig projectionConfig = {})
        : projector_(projectionConfig)
    {
    }

    /** Projects, sorts, and draws a system's star together with its planets, back to front. */
    void drawSystem(
        sf::RenderTarget& target,
        const Planet& star,
        const std::vector<Planet>& planets,
        const Camera& camera
    ) const
    {
        std::vector<const Planet*> bodies;
        bodies.reserve(planets.size() + 1);
        bodies.push_back(&star);

        for (const Planet& planet : planets)
            bodies.push_back(&planet);

        drawBodies(target, bodies, camera);
    }

    /** Projects, sorts, and draws planets alone, without a star. */
    void draw(sf::RenderTarget& target, const std::vector<Planet>& planets, const Camera& camera) const
    {
        std::vector<const Planet*> bodies;
        bodies.reserve(planets.size());

        for (const Planet& planet : planets)
            bodies.push_back(&planet);

        drawBodies(target, bodies, camera);
    }

private:
    struct ProjectedPlanet {
        const Planet* planet = nullptr;
        ProjectedPoint projected;
        float screenRadius = 0.f;

        /** Distance from the camera to the body's centre, used for back-to-front sorting. */
        float distance = 0.f;

        /** False when the centre is behind the camera but part of the sphere is still in front: grid only. */
        bool centreVisible = true;
    };

    /** Below this projected radius, a body is drawn as a marker dot instead of a sphere. */
    static constexpr float dotThreshold = 2.f;

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

    /** Shared draw path for any list of stellar bodies (stars, planets, or both together). */
    void drawBodies(sf::RenderTarget& target, const std::vector<const Planet*>& bodies, const Camera& camera) const
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
        visiblePlanets.reserve(bodies.size());

        for (const Planet* planet : bodies)
        {
            if (planet->radius <= 0.f)
                continue;

            const Vec3 cameraSpace = transformPoint(viewMatrix, planet->position);
            const float distance = length(cameraSpace);

            // Inside (or right on) the sphere there is no meaningful outline to draw.
            if (distance <= planet->radius * 1.0005f)
                continue;

            // Angular radius of a sphere seen from distance d is asin(R / d); on screen that is
            // f * R / sqrt(d^2 - R^2). The old f * R / z badly undersized planets seen up close.
            const float screenRadius =
                planet->radius * focalLength / std::sqrt(distance * distance - planet->radius * planet->radius);

            if (cameraSpace.z <= 1.f)
            {
                // Centre behind the camera, but a big sphere can still wrap around in front of it
                // (skimming low over a planet). Draw its grid lines, which clip properly; skip the
                // outline, which needs a projected centre.
                if (!planet->isStar && cameraSpace.z > -planet->radius)
                    visiblePlanets.push_back({planet, {}, screenRadius, distance, false});

                continue;
            }

            const auto projected = projector_.projectCameraSpace(cameraSpace, camera, viewport, planet->radius);

            if (!projected)
                continue;

            if (projected->position.x < -screenRadius ||
                projected->position.x > viewport.width + screenRadius ||
                projected->position.y < -screenRadius ||
                projected->position.y > viewport.height + screenRadius)
            {
                continue;
            }

            visiblePlanets.push_back({planet, *projected, screenRadius, distance, true});
        }

        std::sort(
            visiblePlanets.begin(),
            visiblePlanets.end(),
            [](const ProjectedPlanet& a, const ProjectedPlanet& b) {
                return a.distance > b.distance;
            }
        );

        for (const ProjectedPlanet& projectedPlanet : visiblePlanets)
            drawPlanet(target, projectedPlanet, camera, viewport, viewMatrix);
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

        if (!projectedPlanet.centreVisible)
        {
            drawSphereGrid(target, planet, camera, viewport, viewMatrix, radius);
            return;
        }

        // Far-off bodies stay on screen as dots, so the whole system can be navigated by eye.
        if (radius < dotThreshold)
        {
            drawMarkerDot(target, center, planet.isStar);
            return;
        }

        if (planet.isStar)
        {
            drawFilledStar(target, center, radius);
            return;
        }

        if (planet.hasRing)
            drawRing(target, planet, center, radius);

        drawSphereGrid(target, planet, camera, viewport, viewMatrix, radius);
        drawSilhouette(target, center, radius);
    }

    /** A small fixed-size dot standing in for a body too distant to resolve. */
    static void drawMarkerDot(sf::RenderTarget& target, sf::Vector2f center, bool isStar)
    {
        const float dotRadius = isStar ? 2.f : 1.5f;
        sf::CircleShape dot(dotRadius, 8);
        dot.setOrigin({dotRadius, dotRadius});
        dot.setPosition(center);
        dot.setFillColor(isStar ? sf::Color::White : sf::Color(255, 255, 255, 200));
        target.draw(dot);
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
        // Up close a big planet fills the screen; a denser grid keeps its curvature (and the
        // sense of speed over it) readable instead of a handful of long straight chords.
        const bool close = screenRadius > 400.f;
        const int segments = close ? 96 : (screenRadius > 80.f ? 72 : 48);
        const int latitudeBands = close ? 12 : (screenRadius > 110.f ? 8 : 6);
        const int meridianBands = close ? 16 : (screenRadius > 110.f ? 10 : 8);

        // Every segment of this planet goes into one vertex batch and one draw call.
        std::vector<sf::Vertex> lines;
        lines.reserve(static_cast<std::size_t>((latitudeBands + meridianBands) * segments * 2));

        for (int latitudeIndex = 1; latitudeIndex < latitudeBands; ++latitudeIndex)
        {
            const float latitude =
                -pi * 0.5f + static_cast<float>(latitudeIndex) * pi / static_cast<float>(latitudeBands);
            drawLatitudeRing(lines, planet, camera, viewport, viewMatrix, latitude, segments);
        }

        for (int meridianIndex = 0; meridianIndex < meridianBands; ++meridianIndex)
        {
            const float longitude =
                static_cast<float>(meridianIndex) * pi / static_cast<float>(meridianBands);
            drawMeridianRing(lines, planet, camera, viewport, viewMatrix, longitude, segments);
        }

        if (!lines.empty())
            target.draw(lines.data(), lines.size(), sf::PrimitiveType::Lines);
    }

    /** Draws the star as a solid white disc instead of the wire grid used for planets. */
    static void drawFilledStar(sf::RenderTarget& target, sf::Vector2f center, float radius)
    {
        sf::CircleShape disc(radius, 96);
        disc.setOrigin({radius, radius});
        disc.setPosition(center);
        disc.setFillColor(sf::Color::White);
        target.draw(disc);
    }

    void drawLatitudeRing(
        std::vector<sf::Vertex>& lines,
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
            drawWireSegment(lines, planet, camera, viewport, viewMatrix, previous, current, 0.86f);
            previous = current;
        }
    }

    void drawMeridianRing(
        std::vector<sf::Vertex>& lines,
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
            drawWireSegment(lines, planet, camera, viewport, viewMatrix, previous, current, 1.f);
            previous = current;
        }
    }

    void drawWireSegment(
        std::vector<sf::Vertex>& lines,
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
        lines.push_back(sf::Vertex({projectedStart->position.x, projectedStart->position.y}, color));
        lines.push_back(sf::Vertex({projectedEnd->position.x, projectedEnd->position.y}, color));
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
        silhouette.setOutlineThickness(std::clamp(radius * 0.006f, 1.f, 3.f));
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