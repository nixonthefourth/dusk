//
// Asteroids and asteroid belts.
//

#ifndef DUSK_ASTEROID_H
#define DUSK_ASTEROID_H

#include "math/Vec3.h++"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <random>
#include <utility>
#include <vector>

/**
 * A low-poly rock: a unit-radius icosphere with every vertex pushed in or out at random. Faces
 * keep outward winding, so the renderer can hide the far side of each rock (hidden-line removal
 * for a single, roughly convex body). Shapes are shared templates; instances scale and spin them.
 */
struct AsteroidShape {
    std::vector<Vec3> vertices;
    std::vector<std::array<int, 3>> faces;

    /** Unique edges, each with the two faces that share it. */
    struct Edge {
        int a = 0;
        int b = 0;
        int faceA = -1;
        int faceB = -1;
    };

    std::vector<Edge> edges;
};

/**
 * One asteroid. Belt rocks are produced on demand by the belt streamer and never stored;
 * free-drifting rocks (World::driftingAsteroids) are the same type, kept and moved each step.
 */
struct Asteroid {
    Vec3 position;

    /** World-space velocity: a belt rock's orbital motion, or a drifting rock's own heading. */
    Vec3 velocity;

    float radius = 100.f;
    int shape = 0;

    /** Unit spin axis, spin rate (rad/s) and starting angle: rocks tumble slowly. */
    Vec3 spinAxis = {0.f, 1.f, 0.f};
    float spinRate = 0.f;
    float spinPhase = 0.f;

    /** Collision radius: a little inside the jagged outline, so grazing a spike doesn't snag. */
    float collisionRadius() const
    {
        return radius * 0.85f;
    }
};

/**
 * A ring of rocks around a centre — the star for a system belt, or a planet for a planet's
 * little debris belt — in a plane parallel to the system plane. The band has radius
 * `centreRadius`, is `halfWidth` across radially and `halfThickness` vertically, and its density
 * falls off smoothly to zero at its edges.
 *
 * Individual rocks aren't stored: they are regenerated deterministically from `seed`, in the
 * belt's own rotating frame, for whatever part of the belt is near the camera or the ship. The
 * whole belt turns about its centre at `angularSpeed`, so every rock orbits.
 */
struct AsteroidBelt {
    /** Index of the planet this belt circles, or -1 for a belt around the star. */
    int hostPlanetIndex = -1;

    /** World position and velocity of the belt's centre (the star, or the host planet), refreshed every step. */
    Vec3 centre;
    Vec3 centreVelocity;

    /** Orbital angular speed about +Y (rad/s; positive matches the planets) and the current angle. */
    float angularSpeed = 0.f;
    float rotation = 0.f;

    /** Multiplier on rock sizes: planet belts are made of smaller rubble. */
    float rockScale = 1.f;

    float centreRadius = 300000.f;
    float halfWidth = 14000.f;
    float halfThickness = 4000.f;

    /** Expected rocks per streaming cell at the very middle of the band (roughly 150-230 rocks in view). */
    float peakDensity = 7.f;

    std::uint32_t seed = 0;

    /** Shape templates: coarse rocks first, then finer ones used for the big boulders. */
    std::vector<AsteroidShape> shapes;
    int coarseShapeCount = 0;

    /** Dust points through the whole band, in the belt's own frame (relative to its centre, before rotation). */
    std::vector<Vec3> dust;
};

/** Rotates a vector about +Y by `angle`, in the same sense the planets orbit. */
inline Vec3 rotateAboutY(const Vec3& v, float angle)
{
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return {v.x * c + v.z * s, v.y, -v.x * s + v.z * c};
}

/** Belt-frame position to world position, at the belt's current rotation. */
inline Vec3 beltToWorld(const AsteroidBelt& belt, const Vec3& local)
{
    return belt.centre + rotateAboutY(local, belt.rotation);
}

/** World position to belt-frame position. */
inline Vec3 worldToBelt(const AsteroidBelt& belt, const Vec3& world)
{
    return rotateAboutY(world - belt.centre, -belt.rotation);
}

/** World velocity of a point riding the belt at world position `world`. */
inline Vec3 beltVelocityAt(const AsteroidBelt& belt, const Vec3& world)
{
    const Vec3 r = world - belt.centre;
    return belt.centreVelocity + Vec3{r.z, 0.f, -r.x} * belt.angularSpeed;
}

/* ---- Shape generation ------------------------------------------------------------------------- */

namespace asteroid_shapes {

/** Base icosahedron: 12 vertices, 20 outward-wound faces. */
inline void icosahedron(std::vector<Vec3>& vertices, std::vector<std::array<int, 3>>& faces)
{
    const float t = (1.f + std::sqrt(5.f)) * 0.5f;

    vertices =
    {
        {-1.f, t, 0.f}, {1.f, t, 0.f}, {-1.f, -t, 0.f}, {1.f, -t, 0.f},
        {0.f, -1.f, t}, {0.f, 1.f, t}, {0.f, -1.f, -t}, {0.f, 1.f, -t},
        {t, 0.f, -1.f}, {t, 0.f, 1.f}, {-t, 0.f, -1.f}, {-t, 0.f, 1.f}
    };

    for (Vec3& vertex : vertices)
        vertex = normalized(vertex);

    faces =
    {
        {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
        {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
        {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
        {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
    };
}

/** Splits every face into four, projecting new vertices onto the unit sphere. */
inline void subdivide(std::vector<Vec3>& vertices, std::vector<std::array<int, 3>>& faces)
{
    std::map<std::pair<int, int>, int> midpoints;

    const auto midpoint = [&](int a, int b)
    {
        const auto key = std::minmax(a, b);
        const auto found = midpoints.find(key);

        if (found != midpoints.end())
            return found->second;

        vertices.push_back(normalized((vertices[static_cast<std::size_t>(a)] + vertices[static_cast<std::size_t>(b)]) * 0.5f));
        const int index = static_cast<int>(vertices.size()) - 1;
        midpoints.emplace(key, index);
        return index;
    };

    std::vector<std::array<int, 3>> refined;
    refined.reserve(faces.size() * 4);

    for (const auto& face : faces)
    {
        const int ab = midpoint(face[0], face[1]);
        const int bc = midpoint(face[1], face[2]);
        const int ca = midpoint(face[2], face[0]);

        refined.push_back({face[0], ab, ca});
        refined.push_back({face[1], bc, ab});
        refined.push_back({face[2], ca, bc});
        refined.push_back({ab, bc, ca});
    }

    faces = std::move(refined);
}

/** Builds a rock from an icosphere: lumpy radii, a random squash, edges with their two faces. */
inline AsteroidShape makeRock(std::mt19937& rng, bool fine)
{
    AsteroidShape shape;
    icosahedron(shape.vertices, shape.faces);

    if (fine)
        subdivide(shape.vertices, shape.faces);

    std::uniform_real_distribution<float> lump(fine ? 0.78f : 0.72f, fine ? 1.12f : 1.15f);
    std::uniform_real_distribution<float> squash(0.7f, 1.f);
    const Vec3 scale = {1.f, squash(rng), squash(rng)};

    for (Vec3& vertex : shape.vertices)
    {
        const float radius = lump(rng);
        vertex = {vertex.x * radius * scale.x, vertex.y * radius * scale.y, vertex.z * radius * scale.z};
    }

    // Keep outward winding after the vertices moved (a face whose normal now points inward is flipped).
    for (auto& face : shape.faces)
    {
        const Vec3& a = shape.vertices[static_cast<std::size_t>(face[0])];
        const Vec3& b = shape.vertices[static_cast<std::size_t>(face[1])];
        const Vec3& c = shape.vertices[static_cast<std::size_t>(face[2])];

        if (dot(cross(b - a, c - a), (a + b + c) / 3.f) < 0.f)
            std::swap(face[1], face[2]);
    }

    std::map<std::pair<int, int>, std::size_t> edgeIndex;

    for (std::size_t faceIndex = 0; faceIndex < shape.faces.size(); ++faceIndex)
    {
        const auto& face = shape.faces[faceIndex];

        for (int corner = 0; corner < 3; ++corner)
        {
            const auto key = std::minmax(face[static_cast<std::size_t>(corner)], face[static_cast<std::size_t>((corner + 1) % 3)]);
            const auto found = edgeIndex.find(key);

            if (found == edgeIndex.end())
            {
                edgeIndex.emplace(key, shape.edges.size());
                shape.edges.push_back({key.first, key.second, static_cast<int>(faceIndex), -1});
            }
            else
            {
                shape.edges[found->second].faceB = static_cast<int>(faceIndex);
            }
        }
    }

    // Normalise so the furthest vertex sits at radius 1: instance radius is then the true size.
    float furthest = 0.f;

    for (const Vec3& vertex : shape.vertices)
        furthest = std::max(furthest, length(vertex));

    if (furthest > 0.f)
    {
        for (Vec3& vertex : shape.vertices)
            vertex = vertex / furthest;
    }

    return shape;
}

} // namespace asteroid_shapes

/** Rotates a vector about a unit axis (Rodrigues' formula). */
inline Vec3 rotateAboutAxis(const Vec3& v, const Vec3& axis, float angle)
{
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return v * c + cross(axis, v) * s + axis * (dot(axis, v) * (1.f - c));
}

#endif //DUSK_ASTEROID_H
