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
#include <algorithm>
#include <set>
#include <utility>
#include <vector>

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
        std::vector<Vec3> cameraVertices;
        cameraVertices.reserve(ship.model.vertices.size());

        for (const Vec3& vertex : ship.model.vertices)
            cameraVertices.push_back(transformPoint(viewMatrix, shipLocalToWorld(ship, vertex)));

        const FaceEdgeVisibility faceEdges = classifyFaceEdges(ship.model, cameraVertices);

        for (const VectorLine& line : ship.model.lines)
        {
            if (line.hideWhenViewedFromAbove && cameraAboveShip)
                continue;

            if (!isValidVertexIndex(line.start, cameraVertices.size()) ||
                !isValidVertexIndex(line.end, cameraVertices.size()))
            {
                continue;
            }

            if (!lineSurvivesFaceCulling(line, faceEdges))
                continue;

            const Vec3 start = cameraVertices[static_cast<std::size_t>(line.start)];
            const Vec3 end = cameraVertices[static_cast<std::size_t>(line.end)];
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
    using EdgeKey = std::pair<int, int>;

    struct FaceEdgeVisibility {
        std::set<EdgeKey> all;
        std::set<EdgeKey> visible;
    };

    Projector projector_;

    static bool isValidVertexIndex(int index, std::size_t vertexCount)
    {
        return index >= 0 && static_cast<std::size_t>(index) < vertexCount;
    }

    static EdgeKey edgeKeyFor(int a, int b)
    {
        return std::minmax(a, b);
    }

    static void addFaceEdges(std::set<EdgeKey>& edges, const VectorFace& face)
    {
        edges.insert(edgeKeyFor(face.a, face.b));
        edges.insert(edgeKeyFor(face.b, face.c));
        edges.insert(edgeKeyFor(face.c, face.a));
    }

    static bool hasArea(const Vec3& a, const Vec3& b, const Vec3& c)
    {
        const Vec3 normal = cross(b - a, c - a);
        return dot(normal, normal) > 0.f;
    }

    FaceEdgeVisibility classifyFaceEdges(
        const VectorModel& model,
        const std::vector<Vec3>& cameraVertices
    ) const
    {
        FaceEdgeVisibility result;

        for (const VectorFace& face : model.faces)
        {
            if (!isValidVertexIndex(face.a, cameraVertices.size()) ||
                !isValidVertexIndex(face.b, cameraVertices.size()) ||
                !isValidVertexIndex(face.c, cameraVertices.size()))
            {
                continue;
            }

            const Vec3& a = cameraVertices[static_cast<std::size_t>(face.a)];
            const Vec3& b = cameraVertices[static_cast<std::size_t>(face.b)];
            const Vec3& c = cameraVertices[static_cast<std::size_t>(face.c)];

            if (!hasArea(a, b, c))
                continue;

            addFaceEdges(result.all, face);

            if (projector_.isFrontFacing(a, b, c))
                addFaceEdges(result.visible, face);
        }

        return result;
    }

    static bool lineSurvivesFaceCulling(
        const VectorLine& line,
        const FaceEdgeVisibility& faceEdges
    )
    {
        if (faceEdges.all.empty())
            return true;

        const EdgeKey edge = edgeKeyFor(line.start, line.end);
        return !faceEdges.all.contains(edge) || faceEdges.visible.contains(edge);
    }
};

#endif //DUSK_SHIP_RENDERER_H
