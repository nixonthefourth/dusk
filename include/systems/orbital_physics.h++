//
// Created by Mykyta Khomiakov on 26/07/2026.
//

#ifndef DUSK_ORBITAL_PHYSICS_H
#define DUSK_ORBITAL_PHYSICS_H

#include "math/Vec3.h++"
#include "math/verlet.h++"
#include "objects/planet.h++"
#include <algorithm>
#include <cmath>
#include <vector>

namespace orbital {
    /** Gravitational constant tuned for this project's world-unit scale, not real SI units. */
    constexpr float g = 600.f;

    /** Softening distance so acceleration doesn't spike toward infinity at very close range. */
    constexpr float minDistance = 200.f;

    /** Returns the acceleration exerted on a point at `from`, by a mass `towardMass` at `toward`. */
    inline Vec3 gravitationalAcceleration(const Vec3& from, const Vec3& toward, float towardMass)
    {
        const Vec3 offset = toward - from;
        const float distance = std::max(length(offset), minDistance);
        const float accelerationMagnitude = g * towardMass / (distance * distance);
        return normalized(offset) * accelerationMagnitude;
    }

    /** Returns the velocity that gives a circular orbit at the current position around a central mass. */
    inline Vec3 circularOrbitVelocity(const Vec3& position, const Vec3& centerPosition, float centerMass)
    {
        const Vec3 radial = position - centerPosition;
        const float distance = std::max(length(radial), minDistance);
        const float speed = std::sqrt(g * centerMass / distance);

        const Vec3 worldUp = {0.f, 1.f, 0.f};
        Vec3 tangent = cross(worldUp, normalized(radial));

        if (length(tangent) == 0.f)
            tangent = {1.f, 0.f, 0.f}; // radial happened to be parallel to world up

        return normalized(tangent) * speed;
    }

    /** Returns the combined gravitational acceleration on one planet from a central mass and all others. */
    inline Vec3 accelerationOnPlanet(
        const Planet& planet,
        const Vec3& centerPosition,
        float centerMass,
        const std::vector<Planet>& planets,
        std::size_t planetIndex
    )
    {
        Vec3 acceleration = gravitationalAcceleration(planet.position, centerPosition, centerMass);

        for (std::size_t otherIndex = 0; otherIndex < planets.size(); ++otherIndex)
        {
            if (otherIndex == planetIndex)
                continue;

            acceleration += gravitationalAcceleration(planet.position, planets[otherIndex].position, planets[otherIndex].mass);
        }

        return acceleration;
    }

    /** Integrates every planet's orbital motion one physics step, around a fixed central mass. */
    inline void integrateOrbitalPhysics(
        std::vector<Planet>& planets,
        const Vec3& centerPosition,
        float centerMass,
        float dt
    )
    {
        std::vector<Vec3> oldAccelerations(planets.size());

        for (std::size_t i = 0; i < planets.size(); ++i)
            oldAccelerations[i] = accelerationOnPlanet(planets[i], centerPosition, centerMass, planets, i);

        for (std::size_t i = 0; i < planets.size(); ++i)
        {
            planets[i].position = verlet::position_update(
                planets[i].position,
                planets[i].velocity,
                oldAccelerations[i],
                dt
            );
        }

        for (std::size_t i = 0; i < planets.size(); ++i)
        {
            const Vec3 newAcceleration = accelerationOnPlanet(planets[i], centerPosition, centerMass, planets, i);

            planets[i].velocity = verlet::velocity_update(
                planets[i].velocity,
                oldAccelerations[i],
                newAcceleration,
                dt
            );
        }
    }

} // namespace orbital

#endif //DUSK_ORBITAL_PHYSICS_H