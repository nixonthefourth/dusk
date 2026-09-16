//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_STATION_H
#define DUSK_STATION_H

#include "math/Vec3.h++"
#include "objects/collision_body.h++"
#include "io/obj_loader.h++"
#include "model/vector_model.h++"
#include <string>

/** A space station: world placement plus an OBJ-loaded vector model. */
struct Station {
    /** Center of the station in world space. */
    Vec3 position = {0.f, 0.f, 4000.f};

    /** Euler rotation in radians. */
    Vec3 rotation;

    /** Edge length in world units. */
    float size = 600.f;

    /** Object-level collision state, refreshed by World every physics update. */
    CollisionBody collision;

    /** Sphere scale used for the station's portal-like collision volume. */
    float collisionRadiusScale = 0.56f;

    /** Base angular speed in radians per second. */
    float rotationSpeed = 0.7f;

    /** Local-space vector model rendered for this station. Empty until an OBJ is loaded. */
    VectorModel model;

    /** Replaces the station's local vector model with an OBJ converted into vertices and edges. */
    bool loadObjModel(const std::string& path, const ObjLoadOptions& options = {})
    {
        const auto loadedModel = loadObjFileAsVectorModel(path, options);

        if (!loadedModel)
            return false;

        model = *loadedModel;
        return true;
    }
};

/** Returns the station's current collision radius, derived from its object size. */
inline float stationCollisionRadius(const Station& station)
{
    return station.size * station.collisionRadiusScale;
}

/** Advances the station rotation with slightly different speeds per axis. */
inline void updateStation(Station& station, float dt)
{
    station.rotation.x += station.rotationSpeed * 0.73f * dt;
    station.rotation.y += station.rotationSpeed * dt;
    station.rotation.z += station.rotationSpeed * 0.41f * dt;
}

#endif //DUSK_STATION_H
