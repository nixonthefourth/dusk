//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_CUBE_RENDERER_H
#define DUSK_CUBE_RENDERER_H

#include "math/Mat4.h++"
#include "math/Vec3.h++"
#include "objects/cube.h++"
#include "rendering/projector.h++"
#include "tools/camera.h++"
#include <SFML/Graphics.hpp>
#include <array>
#include <cmath>

/** Draws a rotating wireframe cube through the same fake-3D projector as stars. */
class CubeRenderer {
public:
    /** Creates a cube renderer with shared projection clipping settings. */
    explicit CubeRenderer(ProjectionConfig projectionConfig = {})
        : projector_(projectionConfig)
    {
    }

    /** Transforms, clips, projects, and draws all cube edges. */
    void draw(sf::RenderTarget& target, const Cube& cube, const Camera& camera) const
    {
        const sf::Vector2u size = target.getSize();
        const Viewport viewport =
        {
            static_cast<float>(size.x),
            static_cast<float>(size.y)
        };

        const Mat4 viewMatrix = projector_.createViewMatrix(camera);
        const std::array<Vec3, 8> vertices = cubeVertices(cube);

        for (const auto& edge : edges)
        {
            const Vec3 start = transformPoint(viewMatrix, vertices[edge[0]]);
            const Vec3 end = transformPoint(viewMatrix, vertices[edge[1]]);
            const auto clipped = projector_.clipLineCameraSpace(start, end, camera, viewport);

            if (!clipped)
                continue;

            const auto projectedStart = projector_.projectCameraSpace(clipped->start, camera, viewport);
            const auto projectedEnd = projector_.projectCameraSpace(clipped->end, camera, viewport);

            if (!projectedStart || !projectedEnd)
                continue;

            sf::Vertex line[] =
            {
                sf::Vertex(
                    {projectedStart->position.x, projectedStart->position.y},
                    sf::Color(80, 220, 255)
                ),
                sf::Vertex(
                    {projectedEnd->position.x, projectedEnd->position.y},
                    sf::Color(255, 240, 120)
                )
            };

            target.draw(line, 2, sf::PrimitiveType::Lines);
        }
    }

private:
    Projector projector_;

    static constexpr std::array<std::array<int, 2>, 12> edges =
    {{
        {{0, 1}}, {{1, 3}}, {{3, 2}}, {{2, 0}},
        {{4, 5}}, {{5, 7}}, {{7, 6}}, {{6, 4}},
        {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}}
    }};

    /** Rotates a local-space point around X, Y, then Z. */
    static Vec3 rotatePoint(Vec3 point, const Vec3& rotation)
    {
        const float sinX = std::sin(rotation.x);
        const float cosX = std::cos(rotation.x);
        point =
        {
            point.x,
            cosX * point.y - sinX * point.z,
            sinX * point.y + cosX * point.z
        };

        const float sinY = std::sin(rotation.y);
        const float cosY = std::cos(rotation.y);
        point =
        {
            cosY * point.x + sinY * point.z,
            point.y,
            -sinY * point.x + cosY * point.z
        };

        const float sinZ = std::sin(rotation.z);
        const float cosZ = std::cos(rotation.z);
        return
        {
            cosZ * point.x - sinZ * point.y,
            sinZ * point.x + cosZ * point.y,
            point.z
        };
    }

    /** Returns all cube vertices in world space after applying rotation and translation. */
    static std::array<Vec3, 8> cubeVertices(const Cube& cube)
    {
        const float halfSize = cube.size * 0.5f;
        const std::array<Vec3, 8> local =
        {{
            {-halfSize, -halfSize, -halfSize},
            { halfSize, -halfSize, -halfSize},
            {-halfSize,  halfSize, -halfSize},
            { halfSize,  halfSize, -halfSize},
            {-halfSize, -halfSize,  halfSize},
            { halfSize, -halfSize,  halfSize},
            {-halfSize,  halfSize,  halfSize},
            { halfSize,  halfSize,  halfSize}
        }};

        std::array<Vec3, 8> vertices;

        for (std::size_t i = 0; i < local.size(); ++i)
            vertices[i] = rotatePoint(local[i], cube.rotation) + cube.position;

        return vertices;
    }
};

#endif //DUSK_CUBE_RENDERER_H
