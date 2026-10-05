//
// System generation: the star, planets on their orbital shells, the station's placement around
// its host, and the player's spawn pose. All of it scaled so planets dwarf the ship.
//

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

    /*
     * System scale. The player ship is ~500 units long and the small station ~1300 units across.
     * Planets run from ~20x to ~70x the ship's length in radius, the star larger still, and orbits
     * are spaced so that crossing between planets is a cruise-drive trip rather than a short hop.
     * Everything stays inside roughly a million units of the origin, where float positions still
     * resolve to a few hundredths of a unit, so nothing jitters on screen.
     */
    constexpr float minStarRadius = 45000.f;
    constexpr float maxStarRadius = 80000.f;

    constexpr float minPlanetRadius = 10000.f;
    constexpr float maxPlanetRadius = 34000.f;

    /**
     * Star mass per unit of radius, given orbital::g = 100. This puts first-shell planets on
     * ~140-180 u/s orbits (slow enough for a station to be chased down and docked with) and gives
     * the star a surface pull of roughly 1 u/s^2: noticeable with flight assist off, harmless with it on.
     */
    constexpr float starMassPerRadius = 750.f;

    /**
     * Planet mass per unit of radius squared (a fixed surface gravity of 0.05 u/s^2 at g = 100).
     * Planets stay around 0.5-2% of their star's mass, so the N-body orbits remain well-behaved
     * instead of the planets visibly tugging each other out of their shells.
     */
    constexpr float planetMassPerRadiusSquared = 0.0005f;

    /** First orbital shell, measured from the star's surface, and the spacing between shells. */
    constexpr float firstShellClearance = 150000.f;
    constexpr float shellStep = 140000.f;
    constexpr float shellJitter = 18000.f;

    /** Generates the system's central star. Always at the origin, never integrated by orbital physics. */
    inline Planet generateStar(std::mt19937& rng)
    {
        std::uniform_real_distribution<float> radiusDist(minStarRadius, maxStarRadius);

        Planet star;
        star.isStar = true;
        star.position = {0.f, 0.f, 0.f};
        star.radius = radiusDist(rng);

        // Bigger stars pull harder; ties orbital speeds to how big this particular star rolled.
        star.mass = star.radius * starMassPerRadius;

        return star;
    }

    /**
     * Generates one planet in an outward shell keyed to its index. With radius up to 34000
     * (diameter 68000), adjacent shells need about 70000 units between centres to stay apart;
     * shellStep minus twice the jitter still leaves 104000, so planets never overlap, and the first
     * shell sits well outside the star's mass-lock zone.
     */
    inline Planet generatePlanet(std::mt19937& rng, int planetIndex, const Planet& star)
    {
        std::uniform_real_distribution<float> angleDist(0.f, 2.f * pi);
        std::uniform_real_distribution<float> heightJitter(-6000.f, 6000.f);
        std::uniform_real_distribution<float> distanceJitter(-shellJitter, shellJitter);
        std::uniform_real_distribution<float> unitDist(0.f, 1.f);
        std::uniform_real_distribution<float> ringRotationDist(-30.f, 30.f);
        std::uniform_real_distribution<float> ringFlatteningDist(0.18f, 0.34f);

        const float shellDistance = star.radius + firstShellClearance + static_cast<float>(planetIndex) * shellStep;
        const float angle = angleDist(rng);
        const float distance = shellDistance + distanceJitter(rng);

        Planet planet;
        planet.position =
        {
            std::sin(angle) * distance,
            heightJitter(rng),
            std::cos(angle) * distance
        };

        // Skewed toward the small end: most worlds are rocky, the odd one is a giant.
        planet.radius = minPlanetRadius + (maxPlanetRadius - minPlanetRadius) * std::pow(unitDist(rng), 1.4f);

        // Mass grows with surface area, so a giant pulls harder without destabilising the system.
        planet.mass = planet.radius * planet.radius * planetMassPerRadiusSquared;

        planet.velocity = orbital::circularOrbitVelocity(planet.position, star.position, star.mass);

        planet.hasRing = unitDist(rng) < 0.3f;

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

    /** Where the player starts in a freshly entered system, and which way the nose points. */
    struct SpawnPose {
        Vec3 position;
        Vec3 facing = {0.f, 0.f, 1.f};
    };

    /** Pushes a point out of whichever body it ended up nearest, to at least `clearance` above the surface. */
    inline Vec3 clearOfNearestBody(Vec3 position, const Planet& star, const std::vector<Planet>& planets, float clearance)
    {
        const Planet* nearest = findNearestBody(position, star, planets);
        const float requiredDistance = nearest->radius + clearance;
        const float currentDistance = length(position - nearest->position);

        if (currentDistance >= requiredDistance)
            return position;

        const Vec3 direction = currentDistance > 0.f
            ? normalized(position - nearest->position)
            : Vec3{0.f, 0.f, -1.f};

        return nearest->position + direction * requiredDistance;
    }

    /**
     * Picks a spawn that shows off the system's scale. With a station, the ship starts a few
     * kilometres out from it on the far side from its host, looking back at the station with the
     * planet filling the view behind it. Without one, it starts between the innermost planet and
     * the star, facing the planet. Either way the first thing on screen is something worth flying to.
     */
    inline SpawnPose shipSpawnPose(
        const Planet& star,
        const std::vector<Planet>& planets,
        bool hasStation,
        const Vec3& stationPosition,
        int stationHostPlanetIndex
    )
    {
        constexpr float stationStandoff = 9000.f;
        constexpr float surfaceClearance = 3000.f;
        const Vec3 worldUp = {0.f, 1.f, 0.f};

        SpawnPose pose;
        Vec3 lookTarget = star.position;

        const bool validHost =
            hasStation &&
            stationHostPlanetIndex >= 0 &&
            static_cast<std::size_t>(stationHostPlanetIndex) < planets.size();

        if (validHost)
        {
            const Planet& host = planets[static_cast<std::size_t>(stationHostPlanetIndex)];
            Vec3 radial = normalized(stationPosition - host.position);

            if (length(radial) == 0.f)
                radial = {0.f, 0.f, -1.f};

            pose.position = stationPosition + radial * stationStandoff + worldUp * 1500.f;
            lookTarget = stationPosition;
        }
        else if (!planets.empty())
        {
            const Planet* innermost = &planets.front();

            for (const Planet& planet : planets)
            {
                if (length(planet.position - star.position) < length(innermost->position - star.position))
                    innermost = &planet;
            }

            Vec3 towardStar = normalized(star.position - innermost->position);

            if (length(towardStar) == 0.f)
                towardStar = {0.f, 0.f, -1.f};

            pose.position = innermost->position + towardStar * (innermost->radius * 2.4f) + worldUp * 2000.f;
            lookTarget = innermost->position;
        }
        else
        {
            pose.position = star.position + Vec3{0.f, 2000.f, -star.radius * 2.5f};
            lookTarget = star.position;
        }

        pose.position = clearOfNearestBody(pose.position, star, planets, surfaceClearance);

        const Vec3 toTarget = lookTarget - pose.position;

        if (length(toTarget) > 0.f)
            pose.facing = normalized(toTarget);

        return pose;
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

        // Altitude scales with the host, so the station hangs low over small worlds and further
        // out over giants, and always well clear of the surface.
        std::uniform_real_distribution<float> altitudeFractionDist(0.25f, 0.55f);
        std::uniform_real_distribution<float> angleDist(0.f, 2.f * pi);

        // Linear speed along the orbit, in world units per second. Picking this (rather than an
        // angular speed) keeps the station dockable however wide its orbit is.
        std::uniform_real_distribution<float> linearSpeedDist(80.f, 180.f);

        placement.orbitRadius = host.radius * (1.f + altitudeFractionDist(rng)) + 2000.f;
        placement.orbitAngle = angleDist(rng);
        placement.orbitSpeed = linearSpeedDist(rng) / placement.orbitRadius;

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