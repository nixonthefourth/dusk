//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_STATION_RENDERER_H
#define DUSK_STATION_RENDERER_H

#include "math/Mat4.h++"
#include "math/Vec3.h++"
#include "model/vector_model.h++"
#include "objects/cube.h++"
#include "rendering/projector.h++"
#include "tools/camera.h++"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <set>
#include <utility>
#include <vector>

/** Draws a station's vector model (loaded from OBJ) through the shared fake-3D projector. */
class StationRenderer {
public:
    /** Creates a station renderer with shared projection clipping settings. */
    explicit StationRenderer(ProjectionConfig projectionConfig = {})
        : projector_(projectionConfig)
    {
    }

    /** Rotates, translates, culls, clips, projects, and draws every model edge. */
    void draw(sf::RenderTarget& target, const Station& station, const Camera& camera) const
    {
        if (station.model.vertices.empty())
            return;

        const sf::Vector2u size = target.getSize();
        const Viewport viewport =
        {
            static_cast<float>(size.x),
            static_cast<float>(size.y)
        };

        const Mat4 viewMatrix = projector_.createViewMatrix(camera);

        std::vector<Vec3> cameraVertices;
        cameraVertices.reserve(station.model.vertices.size());

        for (const Vec3& local : station.model.vertices)
        {
            const Vec3 world = rotatePoint(local, station.rotation) + station.position;
            cameraVertices.push_back(transformPoint(viewMatrix, world));
        }

        const FaceEdgeVisibility faceEdges = classifyFaceEdges(station.model, cameraVertices);

        for (const VectorLine& line : station.model.lines)
        {
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
                    sf::Color(80, 220, 255)
                ),
                sf::Vertex(
                    {projectedEnd->position.x, projectedEnd->position.y},
                    sf::Color(255, 240, 120)
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

    /** Collects every face edge, and separately those belonging to at least one front-facing face. */
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

            const Vec3 normal = cross(b - a, c - a);

            if (dot(normal, normal) <= 0.f)
                continue;

            addFaceEdges(result.all, face);

            if (projector_.isFrontFacing(a, b, c))
                addFaceEdges(result.visible, face);
        }

        return result;
    }

    /** Hides edges whose every adjacent face points away from the camera. */
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
};

#endif //DUSK_STATION_RENDERER_H
