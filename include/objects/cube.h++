//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_STATION_H
#define DUSK_STATION_H

#include "math/Vec3.h++"
#include "objects/collision_body.h++"
#include "io/obj_loader.h++"
#include "model/vector_model.h++"
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>

/**
 * The docking slot of a station, in the station's local model space.
 * `mouth` is the centre of the opening on the hull, `back` the centre of the slot's rear wall,
 * `normal` points out of the slot, and `slotAxis` runs along the slot's long side.
 */
struct DockingPort {
    Vec3 mouth;
    Vec3 back;
    Vec3 normal = {0.f, 0.f, 1.f};
    Vec3 slotAxis = {1.f, 0.f, 0.f};
    float depth = 0.f;
    bool valid = false;
};

/** A space station: world placement, orientation, docking slot, and an OBJ-loaded vector model. */
struct Station {
    /** Center of the station in world space. */
    Vec3 position = {0.f, 0.f, 4000.f};

    /** World-space directions of the model's local X/Y/Z axes. Rebuilt by refreshStationOrientation(). */
    Vec3 axisX = {1.f, 0.f, 0.f};
    Vec3 axisY = {0.f, 1.f, 0.f};
    Vec3 axisZ = {0.f, 0.f, 1.f};

    /** World direction the docking slot should face. Kept pointing away from the host planet. */
    Vec3 dockFacing = {0.f, 0.f, 1.f};

    /** Current spin around the docking axis in radians (Elite-style station rotation). */
    float spinAngle = 0.f;

    /** Spin speed around the docking axis in radians per second. */
    float rotationSpeed = 0.7f;

    /** Nominal size in world units, used for the collision radius. */
    float size = 600.f;

    /** Object-level collision state, refreshed by World every physics update. */
    CollisionBody collision;

    /** Sphere scale used for the station's portal-like collision volume. */
    float collisionRadiusScale = 0.56f;

    /** Docking slot in local model space. Only valid once configureDockingPort() succeeds. */
    DockingPort dockingPort;

    /** Local-space vector model rendered for this station. Empty until an OBJ is loaded. */
    VectorModel model;

    /** Replaces the station's local vector model with an OBJ converted into vertices and edges. */
    bool loadObjModel(const std::string& path, const ObjLoadOptions& options = {})
    {
        const auto loadedModel = loadObjFileAsVectorModel(path, options);

        if (!loadedModel)
            return false;

        model = *loadedModel;
        dockingPort = {};
        return true;
    }
};

/** Rotates v around the unit axis k by angle radians (Rodrigues' rotation formula). */
inline Vec3 rotateAroundAxis(const Vec3& v, const Vec3& k, float angle)
{
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return v * c + cross(k, v) * s + k * (dot(k, v) * (1.f - c));
}

/** Rotates v by the shortest rotation that takes unit vector `from` onto unit vector `to`. */
inline Vec3 rotateFromTo(const Vec3& v, const Vec3& from, const Vec3& to)
{
    const float cosine = std::clamp(dot(from, to), -1.f, 1.f);
    const Vec3 axis = cross(from, to);
    const float axisLength = length(axis);

    if (axisLength < 1e-6f)
    {
        if (cosine > 0.f)
            return v;

        // Opposite vectors: any perpendicular axis gives a valid half-turn.
        Vec3 perpendicular = cross(from, Vec3{0.f, 1.f, 0.f});

        if (length(perpendicular) < 1e-6f)
            perpendicular = cross(from, Vec3{1.f, 0.f, 0.f});

        return rotateAroundAxis(v, normalized(perpendicular), 3.14159265358979323846f);
    }

    return rotateAroundAxis(v, axis / axisLength, std::acos(cosine));
}

/** Converts a local model-space point into world space. */
inline Vec3 stationLocalToWorld(const Station& station, const Vec3& local)
{
    return station.position + station.axisX * local.x + station.axisY * local.y + station.axisZ * local.z;
}

/** Converts a local model-space direction into world space. */
inline Vec3 stationDirectionToWorld(const Station& station, const Vec3& local)
{
    return station.axisX * local.x + station.axisY * local.y + station.axisZ * local.z;
}

/**
 * Rebuilds the orientation basis: first turn the model so its docking normal faces dockFacing,
 * then spin around that facing by spinAngle. Stations without a docking port spin around world up.
 */
inline void refreshStationOrientation(Station& station)
{
    const Vec3 localNormal = station.dockingPort.valid ? station.dockingPort.normal : Vec3{0.f, 1.f, 0.f};
    const Vec3 facing = station.dockingPort.valid ? normalized(station.dockFacing) : Vec3{0.f, 1.f, 0.f};

    const auto orient = [&](const Vec3& unit)
    {
        return rotateAroundAxis(rotateFromTo(unit, localNormal, facing), facing, station.spinAngle);
    };

    station.axisX = orient({1.f, 0.f, 0.f});
    station.axisY = orient({0.f, 1.f, 0.f});
    station.axisZ = orient({0.f, 0.f, 1.f});
}

/**
 * Defines the docking slot from model vertex indices: the ring of vertices around the opening
 * and the matching ring on the slot's rear wall. Returns false if any index is out of range.
 */
inline bool configureDockingPort(
    Station& station,
    std::initializer_list<int> mouthVertices,
    std::initializer_list<int> backVertices
)
{
    const auto centroid = [&](std::initializer_list<int> indices, Vec3& out)
    {
        if (indices.size() == 0)
            return false;

        Vec3 sum{};

        for (int index : indices)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= station.model.vertices.size())
                return false;

            sum += station.model.vertices[static_cast<std::size_t>(index)];
        }

        out = sum / static_cast<float>(indices.size());
        return true;
    };

    DockingPort port;

    if (!centroid(mouthVertices, port.mouth) || !centroid(backVertices, port.back))
        return false;

    const Vec3 outward = port.mouth - port.back;
    port.depth = length(outward);

    if (port.depth <= 0.f)
        return false;

    port.normal = outward / port.depth;

    // The first two mouth vertices lie along the slot's long edge.
    const Vec3 edge =
        station.model.vertices[static_cast<std::size_t>(*(mouthVertices.begin() + 1))] -
        station.model.vertices[static_cast<std::size_t>(*mouthVertices.begin())];
    const Vec3 alongSlot = edge - port.normal * dot(edge, port.normal);

    if (length(alongSlot) <= 0.f)
        return false;

    port.slotAxis = normalized(alongSlot);
    port.valid = true;

    station.dockingPort = port;
    refreshStationOrientation(station);
    return true;
}

/** World-space centre of the docking slot opening. */
inline Vec3 stationDockMouth(const Station& station)
{
    return stationLocalToWorld(station, station.dockingPort.mouth);
}

/** World-space direction pointing out of the docking slot. */
inline Vec3 stationDockNormal(const Station& station)
{
    return normalized(stationDirectionToWorld(station, station.dockingPort.normal));
}

/** World-space direction along the docking slot's long side. */
inline Vec3 stationDockSlotAxis(const Station& station)
{
    return normalized(stationDirectionToWorld(station, station.dockingPort.slotAxis));
}

/** Returns the station's current collision radius, derived from its object size. */
inline float stationCollisionRadius(const Station& station)
{
    return station.size * station.collisionRadiusScale;
}

/** Advances the station's spin around its docking axis. */
inline void updateStation(Station& station, float dt)
{
    constexpr float tau = 6.28318530717958647692f;
    station.spinAngle = std::fmod(station.spinAngle + station.rotationSpeed * dt, tau);
    refreshStationOrientation(station);
}

#endif //DUSK_STATION_H
