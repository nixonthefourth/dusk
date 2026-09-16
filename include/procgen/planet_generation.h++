#ifndef DUSK_PLANET_GENERATION_H
#define DUSK_PLANET_GENERATION_H

#include "math/Vec3.h++"
#include "objects/cube.h++"
#include "objects/planet.h++"
#include "systems/orbital_physics.h++"
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace procgen {

    constexpr float pi = 3.14159265358979323846f;

    /** Generates the system's central star. Always at the origin, never integrated by orbital physics. */
    inline Planet generateStar(std::mt19937& rng)
    {
        std::uniform_real_distribution<float> radiusDist(3000.f, 5600.f);

        Planet star;
        star.isStar = true;
        star.position = {0.f, 0.f, 0.f};
        star.radius = radiusDist(rng);

        // Bigger stars pull harder; ties orbital speeds to how big this particular star rolled.
        star.mass = star.radius * 10.f;

        return star;
    }

    /**
     * Generates one planet in an outward shell keyed to its index. The shell step and jitter are
     * sized so that even two max-radius planets in adjacent shells can't overlap: with radius up
     * to 3200 (diameter 6400) plus a safety buffer, shells need roughly 8000 units of separation,
     * and this reserves ~9000 minimum after jitter.
     */
    inline Planet generatePlanet(std::mt19937& rng, int planetIndex, const Planet& star)
    {
        std::uniform_real_distribution<float> angleDist(0.f, 2.f * pi);
        std::uniform_real_distribution<float> heightJitter(-400.f, 400.f);
        std::uniform_real_distribution<float> distanceJitter(-1000.f, 1000.f);
        std::uniform_real_distribution<float> radiusDist(1200.f, 3200.f);
        std::uniform_real_distribution<float> ringRollChance(0.f, 1.f);
        std::uniform_real_distribution<float> ringRotationDist(-30.f, 30.f);
        std::uniform_real_distribution<float> ringFlatteningDist(0.18f, 0.34f);

        constexpr float shellBase = 9000.f;
        constexpr float shellStep = 9000.f;

        const float shellDistance = shellBase + static_cast<float>(planetIndex) * shellStep;
        const float angle = angleDist(rng);
        const float distance = shellDistance + distanceJitter(rng);

        Planet planet;
        planet.position =
        {
            std::sin(angle) * distance,
            heightJitter(rng),
            std::cos(angle) * distance
        };
        planet.radius = radiusDist(rng);

        // Bigger planets are proportionally more massive, so gravity and visuals scale together.
        planet.mass = planet.radius * 6.f;

        planet.velocity = orbital::circularOrbitVelocity(planet.position, star.position, star.mass);

        planet.hasRing = ringRollChance(rng) < 0.3f;

        if (planet.hasRing)
        {
            planet.ringRotationDegrees = ringRotationDist(rng);
            planet.ringFlattening = ringFlatteningDist(rng);
        }

        return planet;
    }

    /** Where and how a generated station should orbit its chosen host planet. */
    struct StationPlacement {
        Station station;
        int hostPlanetIndex = -1;
        float orbitRadius = 0.f;
        float orbitAngle = 0.f;
        float orbitSpeed = 0.f;
    };

        /** Returns whichever stellar body (star or planet) is closest to a candidate position. */
    inline const Planet* findNearestBody(const Vec3& position, const Planet& star, const std::vector<Planet>& planets)
    {
        const Planet* nearest = &star;
        float nearestDistance = length(position - star.position);

        for (const Planet& planet : planets)
        {
            const float distance = length(position - planet.position);

            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                nearest = &planet;
            }
        }

        return nearest;
    }

    /**
     * Places the ship inside the system, roughly halfway to the innermost planet's orbit, then
     * checks whichever body (star or planet) ends up nearest to that spot. If the ship is closer
     * than 20% of that body's radius beyond its surface, it gets nudged straight out along the
     * same direction until it clears that margin.
     */
    inline Vec3 shipSpawnPosition(const Planet& star, const std::vector<Planet>& planets)
    {
        constexpr float clearanceFraction = 0.02f;

        // Anchor distance: halfway to the innermost planet, or a few star radii out if there are
        // no planets at all, so the initial guess isn't automatically closest to the star.
        float anchorDistance = star.radius * 4.f;

        if (!planets.empty())
        {
            float nearestPlanetDistance = length(planets.front().position - star.position);

            for (const Planet& planet : planets)
                nearestPlanetDistance = std::min(nearestPlanetDistance, length(planet.position - star.position));

            anchorDistance = nearestPlanetDistance * 0.65f;
        }

        Vec3 spawnPosition = star.position + Vec3{0.f, -60.f, -anchorDistance};

        const Planet* nearest = findNearestBody(spawnPosition, star, planets);
        const float requiredDistance = nearest->radius * (1.f + clearanceFraction);
        const float currentDistance = length(spawnPosition - nearest->position);

        if (currentDistance < requiredDistance)
        {
            const Vec3 direction = currentDistance > 0.f
                ? normalized(spawnPosition - nearest->position)
                : Vec3{0.f, 0.f, -1.f};

            spawnPosition = nearest->position + direction * requiredDistance;
        }

        return spawnPosition;
    }

    /** Picks a host planet and an orbital placement for a station, currently drawn as the test cube. */
    inline StationPlacement generateStationPlacement(std::mt19937& rng, const std::vector<Planet>& planets)
    {
        StationPlacement placement;

        if (planets.empty())
            return placement;

        std::uniform_int_distribution<std::size_t> hostIndex(0, planets.size() - 1);
        placement.hostPlanetIndex = static_cast<int>(hostIndex(rng));

        const Planet& host = planets[static_cast<std::size_t>(placement.hostPlanetIndex)];

        std::uniform_real_distribution<float> orbitBufferDist(900.f, 1800.f);
        std::uniform_real_distribution<float> angleDist(0.f, 2.f * pi);
        std::uniform_real_distribution<float> speedDist(0.08f, 0.22f);

        placement.orbitRadius = host.radius + orbitBufferDist(rng);
        placement.orbitAngle = angleDist(rng);
        placement.orbitSpeed = speedDist(rng);

        placement.station.size = std::clamp(host.radius * 0.35f, 300.f, 900.f);
        placement.station.rotationSpeed = 0.15f;
        placement.station.collision.detectsCollisions = false;
        placement.station.position = host.position + Vec3
        {
            std::cos(placement.orbitAngle) * placement.orbitRadius,
            0.f,
            std::sin(placement.orbitAngle) * placement.orbitRadius
        };

        return placement;
    }

    /** Places the ship just outside the star, far enough back that the camera can frame it on spawn. */
    inline Vec3 shipSpawnPosition(float starRadius)
    {
        constexpr float spawnBuffer = 2500.f;
        return {0.f, -60.f, -(starRadius + spawnBuffer)};
    }

} // namespace procgen

#endif //DUSK_PLANET_GENERATION_H