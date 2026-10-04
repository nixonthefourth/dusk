//
// Asteroid belt generation and on-demand asteroid streaming.
//

#ifndef DUSK_ASTEROID_GENERATION_H
#define DUSK_ASTEROID_GENERATION_H

#include "objects/asteroid.h++"
#include "objects/planet.h++"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace procgen {

    /**
     * Belts are streamed in cubic cells of this size. A cell's rocks depend only on the belt seed
     * and the cell's coordinates, so the same rocks reappear every time you come back.
     */
    constexpr float asteroidCellSize = 5000.f;

    /** Number of coarse and fine shape templates per belt, and static dust points per belt. */
    constexpr int coarseRockShapes = 7;
    constexpr int fineRockShapes = 4;
    constexpr int beltDustPoints = 2600;

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

    /**
     * Rock density at a point, as a fraction of the belt's peak: 1 on the belt's centre line,
     * easing to 0 at its radial and vertical edges. The star sits at the origin and the belt in
     * the y = 0 plane, as everything in a system does.
     */
    inline float beltDensityAt(const AsteroidBelt& belt, const Vec3& position)
    {
        const float radial = (std::hypot(position.x, position.z) - belt.centreRadius) / belt.halfWidth;
        const float vertical = position.y / belt.halfThickness;

        if (radial <= -1.f || radial >= 1.f || vertical <= -1.f || vertical >= 1.f)
            return 0.f;

        return (1.f - radial * radial) * (1.f - vertical * vertical);
    }

    /** Distance from a point to the belt's bounding band (zero inside it). */
    inline float distanceToBelt(const AsteroidBelt& belt, const Vec3& position)
    {
        const float radial = std::max(0.f, std::abs(std::hypot(position.x, position.z) - belt.centreRadius) - belt.halfWidth);
        const float vertical = std::max(0.f, std::abs(position.y) - belt.halfThickness);
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

    /**
     * Calls `visit(const Asteroid&)` for every rock in `belt` within `radius` of `centre`. Rocks
     * are regenerated from each cell's seed on every call; nothing is cached or stored, so the
     * belt costs nothing while you're away from it and is identical whenever you return.
     */
    template <typename Visitor>
    void forEachAsteroidNear(const AsteroidBelt& belt, const Vec3& centre, float radius, Visitor&& visit)
    {
        if (belt.shapes.empty() || distanceToBelt(belt, centre) > radius)
            return;

        const float cell = asteroidCellSize;
        const float halfDiagonal = cell * 0.8660254f;

        const auto cellIndex = [cell](float value) { return static_cast<int>(std::floor(value / cell)); };

        const int x0 = cellIndex(centre.x - radius);
        const int x1 = cellIndex(centre.x + radius);
        const int z0 = cellIndex(centre.z - radius);
        const int z1 = cellIndex(centre.z + radius);
        const int y0 = cellIndex(std::max(centre.y - radius, -belt.halfThickness));
        const int y1 = cellIndex(std::min(centre.y + radius, belt.halfThickness));

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

                    if (length(cellCentre - centre) > radius + halfDiagonal)
                        continue;

                    const float expected = belt.peakDensity * beltDensityAt(belt, cellCentre);

                    if (expected <= 0.01f)
                        continue;

                    std::mt19937 rng(cellSeed(belt.seed, ix, iy, iz));
                    std::poisson_distribution<int> countDist(expected);
                    const int count = countDist(rng);

                    for (int n = 0; n < count; ++n)
                    {
                        Asteroid rock;
                        rock.position =
                        {
                            (static_cast<float>(ix) + unit(rng)) * cell,
                            (static_cast<float>(iy) + unit(rng)) * cell,
                            (static_cast<float>(iz) + unit(rng)) * cell
                        };
                        rock.radius = rollRockRadius(rng);

                        const bool fine = rock.radius >= fineRockRadius && belt.coarseShapeCount < static_cast<int>(belt.shapes.size());
                        const int first = fine ? belt.coarseShapeCount : 0;
                        const int available = fine ? static_cast<int>(belt.shapes.size()) - belt.coarseShapeCount : belt.coarseShapeCount;
                        rock.shape = first + std::min(available - 1, static_cast<int>(unit(rng) * static_cast<float>(available)));

                        rock.spinAxis = normalized(Vec3{unit(rng) - 0.5f, unit(rng) - 0.5f, unit(rng) - 0.5f});
                        if (length(rock.spinAxis) == 0.f)
                            rock.spinAxis = {0.f, 1.f, 0.f};

                        // Small rocks tumble faster than big ones.
                        rock.spinRate = (0.05f + 0.35f * unit(rng)) * std::sqrt(120.f / rock.radius);
                        rock.spinPhase = unit(rng) * 6.2831853f;

                        // Thin the edges of the band by position, not just by cell.
                        if (unit(rng) > beltDensityAt(belt, rock.position) / std::max(0.05f, beltDensityAt(belt, cellCentre)))
                            continue;

                        if (length(rock.position - centre) <= radius + rock.radius)
                            visit(rock);
                    }
                }
            }
        }
    }

    /**
     * Places up to `beltCount` belts in the gaps of this system: inside the first planet's orbit
     * (clear of the star's mass-lock zone), between neighbouring planets, or beyond the last one.
     * Each belt is kept well clear of the planets on either side of its gap, allowing for their
     * slight orbital drift. Uses its own RNG stream, so planets and stations are unaffected.
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
            belt.centreRadius = (gap.inner + gap.outer) * 0.5f;
            belt.halfWidth = std::min(available, 10000.f + 7000.f * unit(rng));
            belt.halfThickness = 3000.f + 2500.f * unit(rng);
            belt.peakDensity = 5.f + 5.f * unit(rng);
            belt.seed = rng();

            std::mt19937 shapeRng(belt.seed);

            for (int index = 0; index < coarseRockShapes; ++index)
                belt.shapes.push_back(asteroid_shapes::makeRock(shapeRng, false));

            belt.coarseShapeCount = coarseRockShapes;

            for (int index = 0; index < fineRockShapes; ++index)
                belt.shapes.push_back(asteroid_shapes::makeRock(shapeRng, true));

            // Dust: denser toward the middle of the band, so from afar the belt reads as a soft ring.
            std::normal_distribution<float> spread(0.f, 0.45f);
            belt.dust.reserve(beltDustPoints);

            for (int index = 0; index < beltDustPoints; ++index)
            {
                const float angle = unit(rng) * 6.2831853f;
                const float radial = std::clamp(spread(rng), -1.f, 1.f) * belt.halfWidth;
                const float height = std::clamp(spread(rng), -1.f, 1.f) * belt.halfThickness;
                const float r = belt.centreRadius + radial;
                belt.dust.push_back({std::sin(angle) * r, height, std::cos(angle) * r});
            }

            belts.push_back(std::move(belt));
        }

        return belts;
    }

} // namespace procgen

#endif //DUSK_ASTEROID_GENERATION_H
