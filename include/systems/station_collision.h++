//
// Collision between the player's ship and the station: the hull, and the walls of the docking slot.
//
// The station's own model is the collider. It is a closed solid with outward-facing triangles (the
// slot is a real cavity in it), so a point is inside the station exactly when the nearest triangle
// faces away from it. The ship is tested at a cloud of sample points (its vertices, the midpoint
// of every edge and the centre of every face), so a thin rim can't slip between two of them.
//

#ifndef DUSK_STATION_COLLISION_H
#define DUSK_STATION_COLLISION_H

#include "math/Vec3.h++"
#include "objects/cube.h++"
#include "objects/ship.h++"
#include <algorithm>
#include <cmath>
#include <vector>

namespace station_collision {

    /** How bouncy the hull is: 0 would absorb a hit entirely, 1 would rebound at full speed. */
    constexpr float restitution = 0.3f;

    /** Extra reach, in units, beyond the hull and the ship's own radius before any testing starts. */
    constexpr float broadPhaseMargin = 30.f;

    /** Rough radius of the ship, in units, for the broad phase (half of its diagonal, rounded up). */
    constexpr float shipReach = 320.f;

    /** What a ship-versus-station contact did in one physics step. */
    struct StationImpact {
        bool happened = false;

        /** How fast the ship and the wall closed on each other at the first contact, in units per second. */
        float speed = 0.f;

        /** Where it happened, in world space. */
        Vec3 point;
    };

    /** The nearest point to `p` on triangle abc (Ericson, Real-Time Collision Detection). */
    inline Vec3 closestPointOnTriangle(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c)
    {
        const Vec3 ab = b - a;
        const Vec3 ac = c - a;
        const Vec3 ap = p - a;
        const float d1 = dot(ab, ap);
        const float d2 = dot(ac, ap);

        if (d1 <= 0.f && d2 <= 0.f)
            return a;

        const Vec3 bp = p - b;
        const float d3 = dot(ab, bp);
        const float d4 = dot(ac, bp);

        if (d3 >= 0.f && d4 <= d3)
            return b;

        const float vc = d1 * d4 - d3 * d2;

        if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f)
            return a + ab * (d1 / (d1 - d3));

        const Vec3 cp = p - c;
        const float d5 = dot(ab, cp);
        const float d6 = dot(ac, cp);

        if (d6 >= 0.f && d5 <= d6)
            return c;

        const float vb = d5 * d2 - d1 * d6;

        if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f)
            return a + ac * (d2 / (d2 - d6));

        const float va = d3 * d6 - d5 * d4;

        if (va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f)
            return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));

        const float denominator = 1.f / (va + vb + vc);
        return a + ab * (vb * denominator) + ac * (vc * denominator);
    }

    /** A sample point found inside the station: how deep, and the direction that leads out through the nearest surface. */
    struct Penetration {
        float depth = 0.f;
        Vec3 normal;          // station-local, pointing out of the solid
        Vec3 point;           // the sample point, in world space
    };

    /** Points further than this from the hull surface are never tested for being inside: no contact can be that deep. */
    constexpr float interestRadius = 500.f;

    /** True if a ray from `origin` along `direction` crosses triangle abc (Moller-Trumbore), counting only forward hits. */
    inline bool rayHitsTriangle(const Vec3& origin, const Vec3& direction, const Vec3& a, const Vec3& b, const Vec3& c)
    {
        const Vec3 edge1 = b - a;
        const Vec3 edge2 = c - a;
        const Vec3 pvec = cross(direction, edge2);
        const float determinant = dot(edge1, pvec);

        if (std::abs(determinant) < 1e-9f)
            return false;

        const float inverse = 1.f / determinant;
        const Vec3 tvec = origin - a;
        const float u = dot(tvec, pvec) * inverse;

        if (u < 0.f || u > 1.f)
            return false;

        const Vec3 qvec = cross(tvec, edge1);
        const float v = dot(direction, qvec) * inverse;

        if (v < 0.f || u + v > 1.f)
            return false;

        return dot(edge2, qvec) * inverse > 1e-4f;
    }

    /**
     * If the station-local point lies inside the solid, fills `out` with how deep it is and the
     * direction out through the nearest surface, and returns true.
     *
     * Whether a point is inside is decided by casting three rays in unrelated directions and
     * counting the faces each crosses (an odd count means inside), taking the majority, so a ray
     * that happens to graze an edge can't decide it alone. A simpler test, "does the nearest
     * triangle face away from me", was tried first and misjudged points beside a vertex or edge,
     * where the neighbouring faces disagree. Only points near the hull (within interestRadius) are
     * tested at all.
     */
    inline bool pointInsideStation(const Station& station, const Vec3& local, Penetration& out)
    {
        const auto& vertices = station.model.vertices;
        float nearestSquared = 1e30f;
        Vec3 nearestPoint;
        Vec3 nearestNormal;

        for (const VectorFace& face : station.model.faces)
        {
            const Vec3& a = vertices[static_cast<std::size_t>(face.a)];
            const Vec3& b = vertices[static_cast<std::size_t>(face.b)];
            const Vec3& c = vertices[static_cast<std::size_t>(face.c)];
            const Vec3 closest = closestPointOnTriangle(local, a, b, c);
            const Vec3 offset = local - closest;
            const float distanceSquared = dot(offset, offset);

            if (distanceSquared < nearestSquared)
            {
                nearestSquared = distanceSquared;
                nearestPoint = closest;
                nearestNormal = normalized(cross(b - a, c - a));
            }
        }

        if (nearestSquared > interestRadius * interestRadius)
            return false;

        static const Vec3 directions[3] =
        {
            normalized(Vec3{0.31f, 0.77f, 0.55f}),
            normalized(Vec3{-0.62f, 0.21f, 0.75f}),
            normalized(Vec3{0.18f, -0.69f, -0.70f})
        };

        int insideVotes = 0;

        for (const Vec3& direction : directions)
        {
            int crossings = 0;

            for (const VectorFace& face : station.model.faces)
            {
                if (rayHitsTriangle(local, direction, vertices[static_cast<std::size_t>(face.a)], vertices[static_cast<std::size_t>(face.b)], vertices[static_cast<std::size_t>(face.c)]))
                    ++crossings;
            }

            insideVotes += crossings % 2;
        }

        if (insideVotes < 2)
            return false;

        out.depth = std::sqrt(nearestSquared);

        // Out through the nearest surface; the face normal stands in when the point is right on it.
        out.normal = out.depth > 1e-3f ? (nearestPoint - local) / out.depth : nearestNormal;
        return true;
    }

    /** The cloud of points that stands for the ship's body: vertices, edge midpoints, face centres (world space). */
    inline std::vector<Vec3> shipSamplePoints(const Ship& ship)
    {
        const VectorModel& model = ship.model;
        std::vector<Vec3> points;
        points.reserve(model.vertices.size() + model.lines.size() + model.faces.size());

        for (const Vec3& vertex : model.vertices)
            points.push_back(shipLocalToWorld(ship, vertex));

        const auto valid = [&](int index) { return index >= 0 && static_cast<std::size_t>(index) < model.vertices.size(); };

        for (const VectorLine& line : model.lines)
        {
            if (valid(line.start) && valid(line.end))
                points.push_back(shipLocalToWorld(ship, (model.vertices[static_cast<std::size_t>(line.start)] + model.vertices[static_cast<std::size_t>(line.end)]) * 0.5f));
        }

        for (const VectorFace& face : model.faces)
        {
            if (valid(face.a) && valid(face.b) && valid(face.c))
            {
                points.push_back(shipLocalToWorld(ship, (model.vertices[static_cast<std::size_t>(face.a)] +
                                                         model.vertices[static_cast<std::size_t>(face.b)] +
                                                         model.vertices[static_cast<std::size_t>(face.c)]) / 3.f));
            }
        }

        return points;
    }

    /**
     * Keeps the ship out of the station. The deepest sample point inside the hull is found, the ship
     * is pushed straight out along that surface's normal, and the part of its velocity (relative to
     * the wall, which moves with the station and with the slot's spin) that points into the wall is
     * reversed with some bounce. Repeats a few times for a ship wedged in a corner. The ship's own
     * roll is part of its motion, so a ship rolling in step with the slot meets its walls at rest.
     * Returns the first contact's impact speed, or no impact if nothing was touched.
     */
    inline StationImpact resolveShipStationCollision(Ship& ship, const Station& station, const Vec3& stationVelocity)
    {
        StationImpact impact;

        if (station.model.faces.empty())
            return impact;

        // Broad phase: ignore a ship that can't be touching anything.
        float hullRadius = 0.f;

        for (const Vec3& vertex : station.model.vertices)
            hullRadius = std::max(hullRadius, length(vertex));

        if (length(ship.position - station.position) > hullRadius + shipReach + broadPhaseMargin)
            return impact;

        const Vec3 spin = stationAngularVelocity(station);

        for (int iteration = 0; iteration < 4; ++iteration)
        {
            Penetration deepest;
            bool found = false;

            for (const Vec3& point : shipSamplePoints(ship))
            {
                Penetration candidate;

                if (pointInsideStation(station, stationWorldToLocal(station, point), candidate) && candidate.depth > deepest.depth)
                {
                    deepest = candidate;
                    deepest.point = point;
                    found = true;
                }
            }

            if (!found || deepest.depth < 0.01f)
                break;

            const Vec3 normal = normalized(stationDirectionToWorld(station, deepest.normal));
            ship.position += normal * (deepest.depth + 0.25f);

            // Closing speed at the contact point, with both bodies' rotation counted.
            const Vec3 contact = deepest.point;
            const Vec3 wallVelocity = stationVelocity + cross(spin, contact - station.position);
            const Vec3 pointVelocity = ship.velocity + cross(shipForward(ship) * ship.rollRate, contact - ship.position);
            const float normalSpeed = dot(pointVelocity - wallVelocity, normal);

            if (normalSpeed < 0.f)
            {
                ship.velocity += normal * (-(1.f + restitution) * normalSpeed);

                if (!impact.happened)
                {
                    impact.happened = true;
                    impact.speed = -normalSpeed;
                    impact.point = contact;
                }
            }
        }

        return impact;
    }

} // namespace station_collision

using station_collision::StationImpact;

#endif //DUSK_STATION_COLLISION_H
