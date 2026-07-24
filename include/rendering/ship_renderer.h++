//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_SHIP_RENDERER_H
#define DUSK_SHIP_RENDERER_H

#include "math/Mat4.h++"
#include "math/Vec3.h++"
#include "objects/ship.h++"
#include "rendering/projector.h++"
#include "tools/camera.h++"
#include <SFML/Graphics.hpp>

/** Draws the player ship as clipped, projected vector lines. */
class ShipRenderer {
public:
    /** Creates a renderer using the same projection settings as the rest of the world. */
    explicit ShipRenderer(ProjectionConfig projectionConfig = {})
        : projector_(projectionConfig)
    {
    }

    /** Projects and draws visible ship lines, hiding underside lines from above. */
    void draw(sf::RenderTarget& target, const Ship& ship, const Camera& camera) const
    {
        const sf::Vector2u size = target.getSize();
        const Viewport viewport =
        {
            static_cast<float>(size.x),
            static_cast<float>(size.y)
        };

        const bool cameraAboveShip = dot(camera.position - ship.position, shipUp(ship)) > 0.f;
        const Mat4 viewMatrix = projector_.createViewMatrix(camera);

        for (const VectorLine& line : ship.model.lines)
        {
            if (line.hideWhenViewedFromAbove && cameraAboveShip)
                continue;

            const Vec3 startWorld = shipLocalToWorld(ship, ship.model.vertices[line.start]);
            const Vec3 endWorld = shipLocalToWorld(ship, ship.model.vertices[line.end]);
            const Vec3 start = transformPoint(viewMatrix, startWorld);
            const Vec3 end = transformPoint(viewMatrix, endWorld);
            const auto clipped = projector_.clipLineCameraSpace(start, end, camera, viewport);

            if (!clipped)
                continue;

            const auto projectedStart = projector_.projectCameraSpace(clipped->start, camera, viewport);
            const auto projectedEnd = projector_.projectCameraSpace(clipped->end, camera, viewport);

            if (!projectedStart || !projectedEnd)
                continue;

            sf::Vertex vertices[] =
            {
                sf::Vertex(
                    {projectedStart->position.x, projectedStart->position.y},
                    sf::Color(255, 255, 255)
                ),
                sf::Vertex(
                    {projectedEnd->position.x, projectedEnd->position.y},
                    sf::Color(255, 255, 255)
                )
            };

            target.draw(vertices, 2, sf::PrimitiveType::Lines);
        }
    }

private:
    Projector projector_;
};

#endif //DUSK_SHIP_RENDERER_H
