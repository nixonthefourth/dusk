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
#include <vector>

/** One wireframe edge in the ship model. */
struct ShipLine {
    int start = 0;
    int end = 0;

    /** Underside edges are skipped when viewed from above the ship. */
    bool hideWhenViewedFromAbove = false;
};

/** Vector-array wireframe model inspired by the Elite 1984 Sidewinder. */
struct ShipModel {
    std::vector<Vec3> vertices =
    {
        {0.f, 0.f, 430.f},
        {-260.f, -35.f, 100.f},
        {260.f, -35.f, 100.f},
        {-560.f, -70.f, -300.f},
        {560.f, -70.f, -300.f},
        {0.f, 120.f, -130.f},
        {0.f, -130.f, -130.f},
        {-200.f, 0.f, -460.f},
        {200.f, 0.f, -460.f},
        {0.f, 35.f, -500.f},
        {0.f, 85.f, 140.f}
    };

    std::vector<ShipLine> lines =
    {
        {0, 1, false}, {0, 2, false}, {1, 3, false}, {2, 4, false},
        {3, 7, false}, {4, 8, false}, {7, 9, false}, {8, 9, false},
        {1, 5, false}, {2, 5, false}, {5, 9, false}, {5, 10, false},
        {0, 10, false}, {10, 1, false}, {10, 2, false},
        {0, 6, true}, {1, 6, true}, {2, 6, true}, {3, 6, true},
        {4, 6, true}, {6, 9, true}
    };
};

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

        for (const ShipLine& line : model_.lines)
        {
            if (line.hideWhenViewedFromAbove && cameraAboveShip)
                continue;

            const Vec3 startWorld = shipLocalToWorld(ship, model_.vertices[line.start]);
            const Vec3 endWorld = shipLocalToWorld(ship, model_.vertices[line.end]);
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
    ShipModel model_;
    Projector projector_;
};

#endif //DUSK_SHIP_RENDERER_H
