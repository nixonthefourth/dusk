//
// Asteroid belt generation and on-demand asteroid streaming.
//

#ifndef DUSK_ASTEROID_GENERATION_H
#define DUSK_ASTEROID_GENERATION_H

#include "objects/asteroid.h++"
#include "objects/planet.h++"
#include "systems/orbital_physics.h++"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace procgen {

    /**
     * Belts are streamed in cubic cells of this size, laid out in each belt's own rotating frame.
     * A cell's rocks depend only on the belt seed and the cell's coordinates, so the same rocks
     * come round again every orbit and every time you return.
     */
    constexpr float asteroidCellSize = 5000.f;

    /** Shape templates per system belt, and static dust points per system belt. */
    constexpr int coarseRockShapes = 7;
    constexpr int fineRockShapes = 4;
    constexpr int beltDustPoints = 2600;

    /** Planet debris belts are smaller: fewer shapes and less dust. */
    constexpr int planetBeltCoarseShapes = 5;
    constexpr int planetBeltFineShapes = 2;
    constexpr int planetBeltDustPoints = 900;

    /** Chance that a planet (other than the station's host) has a little debris belt of its own. */
    constexpr float planetBeltChance = 0.35f;

    /** Rocks at or above this radius use the finer (subdivided) shapes. */
    constexpr float fineRockRadius = 450.f;

    /** Integer hash (a splitmix-style finaliser) used to seed each cell. */
    inline std::uint32_t mixHash(std::uint32_t value)
    {
        value ^= value >> 16;
        value *= 0x7feb352dU;
        value ^= value >> 15;
        value *= 0x846ca68bU;
        value ^= value >> 16;
        return value;
    }

    inline std::uint32_t cellSeed(std::uint32_t beltSeed, int x, int y, int z)
    {
        std::uint32_t h = mixHash(beltSeed ^ 0x9E3779B9U);
        h = mixHash(h ^ static_cast<std::uint32_t>(x) * 0x85EBCA6BU);
        h = mixHash(h ^ static_cast<std::uint32_t>(y) * 0xC2B2AE35U);
        h = mixHash(h ^ static_cast<std::uint32_t>(z) * 0x27D4EB2FU);
        return h;
    }

    /** Density at a belt-frame position, as a fraction of the peak: 1 on the centre line, 0 at the edges. */
    inline float beltDensityLocal(const AsteroidBelt& belt, const Vec3& local)
    {
        const float radial = (std::hypot(local.x, local.z) - belt.centreRadius) / belt.halfWidth;
        const float vertical = local.y / belt.halfThickness;

        if (radial <= -1.f || radial >= 1.f || vertical <= -1.f || vertical >= 1.f)
            return 0.f;

        return (1.f - radial * radial) * (1.f - vertical * vertical);
    }

    /** Density at a world position. The band is symmetric about its axis, so rotation doesn't matter here. */
    inline float beltDensityAt(const AsteroidBelt& belt, const Vec3& position)
    {
        return beltDensityLocal(belt, position - belt.centre);
    }

    /** Distance from a world point to the belt's bounding band (zero inside it). */
    inline float distanceToBelt(const AsteroidBelt& belt, const Vec3& position)
    {
        const Vec3 local = position - belt.centre;
        const float radial = std::max(0.f, std::abs(std::hypot(local.x, local.z) - belt.centreRadius) - belt.halfWidth);
        const float vertical = std::max(0.f, std::abs(local.y) - belt.halfThickness);
        return std::hypot(radial, vertical);
    }

    /** Rolls a rock radius: mostly small, a few boulders, and the odd giant to steer around. */
    inline float rollRockRadius(std::mt19937& rng)
    {
        std::uniform_real_distribution<float> unit(0.f, 1.f);

        if (unit(rng) < 0.012f)
            return 1500.f + 1100.f * unit(rng);

        return 70.f + 1050.f * std::pow(unit(rng), 3.f);
    }

    /** Picks a shape index for a rock: fine shapes for boulders, coarse ones for everything else. */
    inline int pickRockShape(std::mt19937& rng, float radius, int coarseCount, int totalCount)
    {
        std::uniform_real_distribution<float> unit(0.f, 1.f);
        const bool fine = radius >= fineRockRadius && coarseCount < totalCount;
        const int first = fine ? coarseCount : 0;
        const int available = std::max(1, fine ? totalCount - coarseCount : coarseCount);
        return first + std::min(available - 1, static_cast<int>(unit(rng) * static_cast<float>(available)));
    }

    /** Fills in a rock's tumble: a random axis, a rate that is faster for small rocks, a start angle. */
    inline void rollRockSpin(std::mt19937& rng, Asteroid& rock)
    {
        std::uniform_real_distribution<float> unit(0.f, 1.f);
        rock.spinAxis = normalized(Vec3{unit(rng) - 0.5f, unit(rng) - 0.5f, unit(rng) - 0.5f});

        if (length(rock.spinAxis) == 0.f)
            rock.spinAxis = {0.f, 1.f, 0.f};

        rock.spinRate = (0.05f + 0.35f * unit(rng)) * std::sqrt(120.f / rock.radius);
        rock.spinPhase = unit(rng) * 6.2831853f;
    }

    /**
     * Calls `visit(const Asteroid&)` for every rock in `belt` within `radius` of the world point
     * `centre`. The query is moved into the belt's rotating frame, the rocks of the cells around
     * it are regenerated from their seeds, and each one is handed back in world space with its
     * orbital velocity. Nothing is cached or stored.
     */
    template <typename Visitor>
    void forEachAsteroidNear(const AsteroidBelt& belt, const Vec3& centre, float radius, Visitor&& visit)
    {
        if (belt.shapes.empty() || distanceToBelt(belt, centre) > radius)
            return;

        const Vec3 query = worldToBelt(belt, centre);
        const float cell = asteroidCellSize;
        const float halfDiagonal = cell * 0.8660254f;
        const int shapeCount = static_cast<int>(belt.shapes.size());

        const auto cellIndex = [cell](float value) { return static_cast<int>(std::floor(value / cell)); };

        const int x0 = cellIndex(query.x - radius);
        const int x1 = cellIndex(query.x + radius);
        const int z0 = cellIndex(query.z - radius);
        const int z1 = cellIndex(query.z + radius);
        const int y0 = cellIndex(std::max(query.y - radius, -belt.halfThickness));
        const int y1 = cellIndex(std::min(query.y + radius, belt.halfThickness));

        std::uniform_real_distribution<float> unit(0.f, 1.f);

        for (int ix = x0; ix <= x1; ++ix)
        {
            for (int iz = z0; iz <= z1; ++iz)
            {
                const float cx = (static_cast<float>(ix) + 0.5f) * cell;
                const float cz = (static_cast<float>(iz) + 0.5f) * cell;
                const float ringDistance = std::abs(std::hypot(cx, cz) - belt.centreRadius);

                if (ringDistance > belt.halfWidth + halfDiagonal)
                    continue;

                for (int iy = y0; iy <= y1; ++iy)
                {
                    const Vec3 cellCentre = {cx, (static_cast<float>(iy) + 0.5f) * cell, cz};

                    if (length(cellCentre - query) > radius + halfDiagonal)
                        continue;

                    const float cellDensity = beltDensityLocal(belt, cellCentre);
                    const float expected = belt.peakDensity * cellDensity;

                    if (expected <= 0.01f)
                        continue;

                    std::mt19937 rng(cellSeed(belt.seed, ix, iy, iz));
                    std::poisson_distribution<int> countDist(expected);
                    const int count = countDist(rng);

                    for (int n = 0; n < count; ++n)
                    {
                        const Vec3 local =
                        {
                            (static_cast<float>(ix) + unit(rng)) * cell,
                            (static_cast<float>(iy) + unit(rng)) * cell,
                            (static_cast<float>(iz) + unit(rng)) * cell
                        };

                        Asteroid rock;
                        rock.radius = rollRockRadius(rng) * belt.rockScale;
                        rock.shape = pickRockShape(rng, rock.radius / belt.rockScale, belt.coarseShapeCount, shapeCount);
                        rollRockSpin(rng, rock);

                        // Thin the edges of the band by position, not just by cell.
                        if (unit(rng) > beltDensityLocal(belt, local) / std::max(0.05f, cellDensity))
                            continue;

                        if (length(local - query) > radius + rock.radius)
                            continue;

                        rock.position = beltToWorld(belt, local);
                        rock.velocity = beltVelocityAt(belt, rock.position);
                        visit(rock);
                    }
                }
            }
        }
    }

    /** Builds a belt's shape templates and dust from its seed. */
    inline void buildBeltContents(AsteroidBelt& belt, int coarseShapes, int fineShapes, int dustPoints, std::mt19937& rng)
    {
        std::mt19937 shapeRng(belt.seed);

        for (int index = 0; index < coarseShapes; ++index)
            belt.shapes.push_back(asteroid_shapes::makeRock(shapeRng, false));

        belt.coarseShapeCount = coarseShapes;

        for (int index = 0; index < fineShapes; ++index)
            belt.shapes.push_back(asteroid_shapes::makeRock(shapeRng, true));

        // Dust is denser toward the middle of the band, so from afar the belt reads as a soft ring.
        std::uniform_real_distribution<float> unit(0.f, 1.f);
        std::normal_distribution<float> spread(0.f, 0.45f);
        belt.dust.reserve(static_cast<std::size_t>(dustPoints));

        for (int index = 0; index < dustPoints; ++index)
        {
            const float angle = unit(rng) * 6.2831853f;
            const float radial = std::clamp(spread(rng), -1.f, 1.f) * belt.halfWidth;
            const float height = std::clamp(spread(rng), -1.f, 1.f) * belt.halfThickness;
            const float r = belt.centreRadius + radial;
            belt.dust.push_back({std::sin(angle) * r, height, std::cos(angle) * r});
        }
    }

    /**
     * Places up to `beltCount` belts around the star in the gaps of this system: inside the first
     * planet's orbit (clear of the star's mass-lock zone), between neighbouring planets, or beyond
     * the last one, each well clear of the planets on either side. Each belt orbits the star at
     * the Keplerian rate for its radius. Uses its own RNG stream, so planets and stations are
     * unaffected.
     */
    inline std::vector<AsteroidBelt> generateAsteroidBelts(
        std::uint32_t systemSeed,
        int beltCount,
        const Planet& star,
        const std::vector<Planet>& planets
    )
    {
        std::vector<AsteroidBelt> belts;

        if (beltCount <= 0)
            return belts;

        struct Gap {
            float inner = 0.f;
            float outer = 0.f;
        };

        std::vector<std::pair<float, float>> orbits; // (orbital radius, planet radius)

        for (const Planet& planet : planets)
            orbits.emplace_back(length(planet.position - star.position), planet.radius);

        std::sort(orbits.begin(), orbits.end());

        std::vector<Gap> gaps;
        float previousEdge = star.radius * 1.5f + 8000.f;

        for (const auto& [orbit, radius] : orbits)
        {
            gaps.push_back({previousEdge, orbit - radius});
            previousEdge = orbit + radius;
        }

        gaps.push_back({previousEdge, previousEdge + 130000.f});

        constexpr float planetMargin = 14000.f;
        constexpr float minHalfWidth = 6000.f;

        std::mt19937 rng(systemSeed ^ 0xA57E401DU);
        std::uniform_real_distribution<float> unit(0.f, 1.f);
        std::shuffle(gaps.begin(), gaps.end(), rng);

        for (const Gap& gap : gaps)
        {
            if (static_cast<int>(belts.size()) >= beltCount)
                break;

            const float available = (gap.outer - gap.inner) * 0.5f - planetMargin;

            if (available < minHalfWidth)
                continue;

            AsteroidBelt belt;
            belt.centre = star.position;
            belt.centreRadius = (gap.inner + gap.outer) * 0.5f;
            belt.halfWidth = std::min(available, 10000.f + 7000.f * unit(rng));
            belt.halfThickness = 3000.f + 2500.f * unit(rng);
            belt.peakDensity = 5.f + 5.f * unit(rng);
            belt.seed = rng();

            // A real belt moves at orbital speed for its radius: about 150-190 u/s, a full turn
            // taking an hour or more, but plainly visible as rocks drift past.
            belt.angularSpeed = std::sqrt(orbital::g * star.mass / (belt.centreRadius * belt.centreRadius * belt.centreRadius));

            buildBeltContents(belt, coarseRockShapes, fineRockShapes, beltDustPoints, rng);
            belts.push_back(std::move(belt));
        }

        return belts;
    }

    /**
     * Gives some planets a little debris belt of their own: a narrow, thin band of smaller rubble
     * circling just outside the planet, carried along its orbit. The station's host never gets
     * one (its orbit would run through it), and each belt is shrunk, or dropped, to stay well
     * clear of neighbouring planets, the star's lock zone and the system's own belts.
     */
    inline std::vector<AsteroidBelt> generatePlanetBelts(
        std::uint32_t systemSeed,
        const Planet& star,
        const std::vector<Planet>& planets,
        const std::vector<AsteroidBelt>& systemBelts,
        int stationHostPlanetIndex
    )
    {
        std::vector<AsteroidBelt> belts;
        std::mt19937 rng(systemSeed ^ 0x51A7B3C5U);
        std::uniform_real_distribution<float> unit(0.f, 1.f);

        for (std::size_t index = 0; index < planets.size(); ++index)
        {
            // Every roll happens for every planet, so one planet's result never shifts another's.
            const bool rolled = unit(rng) < planetBeltChance;
            const float centreRoll = unit(rng);
            const float widthRoll = unit(rng);
            const float thicknessRoll = unit(rng);
            const float densityRoll = unit(rng);
            const float speedRoll = unit(rng);
            const std::uint32_t seed = rng();

            if (!rolled || static_cast<int>(index) == stationHostPlanetIndex)
                continue;

            const Planet& planet = planets[index];
            const float orbit = length(planet.position - star.position);

            // How far out from the planet's centre the belt may reach.
            float maxOuter = orbit - star.radius * 1.5f - 8000.f;

            for (std::size_t other = 0; other < planets.size(); ++other)
            {
                if (other != index)
                    maxOuter = std::min(maxOuter, length(planets[other].position - planet.position) - planets[other].radius - 12000.f);
            }

            for (const AsteroidBelt& belt : systemBelts)
                maxOuter = std::min(maxOuter, std::abs(orbit - belt.centreRadius) - belt.halfWidth - 10000.f);

            AsteroidBelt belt;
            belt.hostPlanetIndex = static_cast<int>(index);
            belt.centre = planet.position;
            belt.centreVelocity = planet.velocity;
            belt.centreRadius = planet.radius * (1.75f + 0.25f * centreRoll) + 3000.f;
            // A fixed base plus a share of the planet's size, so small (and most common) worlds still get a proper band.
            belt.halfWidth = std::min(2500.f + planet.radius * (0.06f + 0.06f * widthRoll), maxOuter - belt.centreRadius);

            if (belt.halfWidth < 2500.f)
                continue;

            belt.halfThickness = 900.f + 700.f * thicknessRoll;
            belt.peakDensity = 12.f + 8.f * densityRoll;
            belt.rockScale = 0.55f;
            belt.seed = seed;

            // Planet gravity is kept tiny for N-body stability, so a true orbit here would barely
            // move; the rubble is given a brisk 60-110 u/s instead, so the belt visibly turns.
            belt.angularSpeed = (60.f + 50.f * speedRoll) / belt.centreRadius;

            std::mt19937 dustRng(seed ^ 0x2545F491U);
            buildBeltContents(belt, planetBeltCoarseShapes, planetBeltFineShapes, planetBeltDustPoints, dustRng);
            belts.push_back(std::move(belt));
        }

        return belts;
    }

    /** Shared shape templates for free-drifting rocks (not tied to any belt). */
    inline std::vector<AsteroidShape> generateLooseRockShapes(std::uint32_t systemSeed, int coarse, int fine)
    {
        std::vector<AsteroidShape> shapes;
        std::mt19937 rng(systemSeed ^ 0x6C8E9CF5U);

        for (int index = 0; index < coarse; ++index)
            shapes.push_back(asteroid_shapes::makeRock(rng, false));

        for (int index = 0; index < fine; ++index)
            shapes.push_back(asteroid_shapes::makeRock(rng, true));

        return shapes;
    }

} // namespace procgen

#endif //DUSK_ASTEROID_GENERATION_H
