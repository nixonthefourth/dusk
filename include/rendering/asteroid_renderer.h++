//
// Draws asteroid belts: distant dust, and streamed wireframe rocks near the camera.
//

#ifndef DUSK_ASTEROID_RENDERER_H
#define DUSK_ASTEROID_RENDERER_H

#include "math/Mat4.h++"
#include "objects/asteroid.h++"
#include "procgen/asteroid_generation.h++"
#include "rendering/projector.h++"
#include "tools/camera.h++"
#include "world/world.h++"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

/**
 * Two layers per belt:
 *
 *  - Dust: a fixed scatter of points through the whole band, projected with the bodies' far
 *    plane, so a belt reads as a faint ring from anywhere in the system.
 *  - Rocks: every belt rock within `drawDistance` of the camera, regenerated from the belt seed
 *    each frame in the belt's rotating frame, plus the lone drifting rocks. Each rock is a tumbling low-poly shape with its far side hidden (an edge is drawn
 *    only if a face it borders faces the camera). Distant rocks fade in rather than popping, tiny
 *    ones become dots, and small ones switch to the coarse shapes. Every rock line goes into one
 *    vertex batch and one draw call.
 */
class AsteroidRenderer {
public:
    /** Rocks further than this from the camera aren't generated or drawn. */
    static constexpr float drawDistance = 32000.f;

    AsteroidRenderer(ProjectionConfig rockProjection = {}, ProjectionConfig dustProjection = {})
        : rockProjector_(rockProjection), dustProjector_(dustProjection)
    {
    }

    void draw(sf::RenderTarget& target, const World& world, const Camera& camera) const
    {
        if (world.asteroidBelts.empty() && world.driftingAsteroids.empty())
            return;

        const sf::Vector2u size = target.getSize();
        const Viewport viewport = {static_cast<float>(size.x), static_cast<float>(size.y)};
        const Mat4 viewMatrix = rockProjector_.createViewMatrix(camera);

        drawDust(target, world, camera, viewport, viewMatrix);
        drawRocks(target, world, camera, viewport, viewMatrix);
    }

private:
    Projector rockProjector_;
    Projector dustProjector_;

    /**
     * There's no depth buffer, so a point behind the star or a planet would be drawn over it.
     * This hides any point whose line of sight passes through a body that sits in front of it.
     */
    static bool hiddenBehindBody(const World& world, const Vec3& eye, const Vec3& point)
    {
        const Vec3 sight = point - eye;
        const float sightLength = length(sight);

        if (sightLength <= 0.f)
            return false;

        const Vec3 direction = sight / sightLength;

        const auto blocks = [&](const Planet& body)
        {
            if (body.radius <= 0.f)
                return false;

            const Vec3 toBody = body.position - eye;
            const float along = dot(toBody, direction);

            if (along <= 0.f || along - body.radius >= sightLength)
                return false;

            return dot(toBody, toBody) - along * along < body.radius * body.radius;
        };

        if (world.star.isStar && blocks(world.star))
            return true;

        return std::any_of(world.planets.begin(), world.planets.end(), blocks);
    }

    static float focalLength(const Camera& camera, const Viewport& viewport)
    {
        constexpr float degreesToRadians = 3.14159265358979323846f / 180.f;
        return (viewport.height * 0.5f) / std::tan(camera.fov * degreesToRadians * 0.5f);
    }

    void drawDust(
        sf::RenderTarget& target,
        const World& world,
        const Camera& camera,
        const Viewport& viewport,
        const Mat4& viewMatrix
    ) const
    {
        std::vector<sf::Vertex> points;

        for (const AsteroidBelt& belt : world.asteroidBelts)
        {
            for (const Vec3& localMote : belt.dust)
            {
                const Vec3 mote = beltToWorld(belt, localMote);
                const Vec3 cameraSpace = transformPoint(viewMatrix, mote);

                if (cameraSpace.z <= 1.f)
                    continue;

                const auto projected = dustProjector_.projectCameraSpace(cameraSpace, camera, viewport);

                if (projected && hiddenBehindBody(world, camera.position, mote))
                    continue;

                if (!projected)
                    continue;

                points.push_back(sf::Vertex({projected->position.x, projected->position.y}, sf::Color(120, 115, 105)));
            }
        }

        if (!points.empty())
            target.draw(points.data(), points.size(), sf::PrimitiveType::Points);
    }

    /** Scratch buffers reused across rocks within one frame. */
    struct RockBatch {
        std::vector<sf::Vertex> lines;
        std::vector<sf::Vertex> dots;
        std::vector<Vec3> cameraVertices;
        std::vector<bool> faceVisible;
    };

    void drawRocks(
        sf::RenderTarget& target,
        const World& world,
        const Camera& camera,
        const Viewport& viewport,
        const Mat4& viewMatrix
    ) const
    {
        RockBatch batch;
        const float focal = focalLength(camera, viewport);
        const float time = static_cast<float>(std::fmod(world.elapsedTime, 100000.0));

        for (const AsteroidBelt& belt : world.asteroidBelts)
        {
            procgen::forEachAsteroidNear(belt, camera.position, drawDistance, [&](const Asteroid& rock)
            {
                drawRock(batch, world, camera, viewport, viewMatrix, focal, time, rock, belt.shapes, belt.coarseShapeCount);
            });
        }

        for (const Asteroid& rock : world.driftingAsteroids)
        {
            if (length(rock.position - camera.position) <= drawDistance + rock.radius)
                drawRock(batch, world, camera, viewport, viewMatrix, focal, time, rock, world.looseRockShapes, world.looseCoarseShapeCount);
        }

        if (!batch.dots.empty())
            target.draw(batch.dots.data(), batch.dots.size(), sf::PrimitiveType::Points);

        if (!batch.lines.empty())
            target.draw(batch.lines.data(), batch.lines.size(), sf::PrimitiveType::Lines);
    }

    /** Adds one rock to the batch: a fading dot when tiny, otherwise its tumbling shape with the far side hidden. */
    void drawRock(
        RockBatch& batch,
        const World& world,
        const Camera& camera,
        const Viewport& viewport,
        const Mat4& viewMatrix,
        float focal,
        float time,
        const Asteroid& rock,
        const std::vector<AsteroidShape>& shapes,
        int coarseShapeCount
    ) const
    {
        if (shapes.empty())
            return;

        const Vec3 centre = transformPoint(viewMatrix, rock.position);

        if (centre.z + rock.radius <= 1.f)
            return;

        const float distance = length(centre);

        // Fade in over the last 35% of the draw distance, so rocks never pop into view.
        const float fade = std::clamp((drawDistance - distance) / (drawDistance * 0.35f), 0.f, 1.f);

        if (fade <= 0.f)
            return;

        if (hiddenBehindBody(world, camera.position, rock.position))
            return;

        const float depth = std::max(centre.z, 1.f);
        const float screenRadius = rock.radius * focal / depth;

        if (centre.z > rock.radius)
        {
            const auto projected = rockProjector_.projectCameraSpace(centre, camera, viewport, rock.radius);

            if (!projected)
                return;

            if (projected->position.x < -screenRadius || projected->position.x > viewport.width + screenRadius ||
                projected->position.y < -screenRadius || projected->position.y > viewport.height + screenRadius)
            {
                return;
            }

            // Too small to show a shape: a dot.
            if (screenRadius < 1.5f)
            {
                const auto alpha = static_cast<std::uint8_t>(200.f * fade);
                batch.dots.push_back(sf::Vertex({projected->position.x, projected->position.y}, sf::Color(200, 195, 185, alpha)));
                return;
            }
        }

        // Small on screen: a coarse shape is plenty.
        int shapeIndex = std::clamp(rock.shape, 0, static_cast<int>(shapes.size()) - 1);

        if (screenRadius < 6.f && coarseShapeCount > 0)
            shapeIndex = shapeIndex % coarseShapeCount;

        const AsteroidShape& shape = shapes[static_cast<std::size_t>(shapeIndex)];
        const float angle = rock.spinPhase + rock.spinRate * time;

        batch.cameraVertices.clear();

        for (const Vec3& vertex : shape.vertices)
        {
            const Vec3 worldVertex = rock.position + rotateAboutAxis(vertex * rock.radius, rock.spinAxis, angle);
            batch.cameraVertices.push_back(transformPoint(viewMatrix, worldVertex));
        }

        // A face is visible when its outward normal points back toward the camera (origin).
        batch.faceVisible.assign(shape.faces.size(), false);

        for (std::size_t index = 0; index < shape.faces.size(); ++index)
        {
            const auto& face = shape.faces[index];
            const Vec3& a = batch.cameraVertices[static_cast<std::size_t>(face[0])];
            const Vec3& b = batch.cameraVertices[static_cast<std::size_t>(face[1])];
            const Vec3& c = batch.cameraVertices[static_cast<std::size_t>(face[2])];
            batch.faceVisible[index] = dot(cross(b - a, c - a), a) < 0.f;
        }

        const auto alpha = static_cast<std::uint8_t>(235.f * fade);
        const sf::Color color(210, 205, 195, alpha);

        for (const AsteroidShape::Edge& edge : shape.edges)
        {
            const bool visible =
                (edge.faceA >= 0 && batch.faceVisible[static_cast<std::size_t>(edge.faceA)]) ||
                (edge.faceB >= 0 && batch.faceVisible[static_cast<std::size_t>(edge.faceB)]);

            if (!visible)
                continue;

            const auto clipped = rockProjector_.clipLineCameraSpace(
                batch.cameraVertices[static_cast<std::size_t>(edge.a)],
                batch.cameraVertices[static_cast<std::size_t>(edge.b)],
                camera,
                viewport
            );

            if (!clipped)
                continue;

            const auto start = rockProjector_.projectCameraSpace(clipped->start, camera, viewport);
            const auto end = rockProjector_.projectCameraSpace(clipped->end, camera, viewport);

            if (!start || !end)
                continue;

            batch.lines.push_back(sf::Vertex({start->position.x, start->position.y}, color));
            batch.lines.push_back(sf::Vertex({end->position.x, end->position.y}, color));
        }
    }
};

#endif //DUSK_ASTEROID_RENDERER_H
