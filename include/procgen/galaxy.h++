//
// Created by Mykyta Khomiakov on 26/07/2026.
//

#ifndef DUSK_GALAXY_H
#define DUSK_GALAXY_H

#include "statistical.h++"
#include "math/Vec2.h++"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

/** Derives a stable per-system seed from the procgen seed and system index. */
inline std::uint32_t deriveSystemSeed(std::uint32_t gameSeed, int systemIndex)
{
    std::seed_seq seedSequence{gameSeed, static_cast<std::uint32_t>(systemIndex)};
    std::array<std::uint32_t, 1> output{};
    seedSequence.generate(output.begin(), output.end());
    return output[0];
}

/** Owns the procgen seed and the full "on paper" roster of systems generated from it. */
struct Galaxy {
    std::uint32_t seed = 0;
    std::vector<SystemInfo> systems;
};

/** Radius of the galactic disc on the chart, in light years. */
constexpr float galaxyRadius = 500.f;

/** Systems are kept at least this far apart on the chart, so each one stays clickable. */
constexpr float minSystemSpacing = 6.f;

/**
 * Lays the systems out as a two-armed spiral disc with a central bulge, from its own RNG stream,
 * so chart positions never disturb the per-system seeds. System 0, where the player starts, sits
 * out on the rim: the galactic core, the game's end goal, is as far away as it can be.
 */
inline void generateGalaxyLayout(Galaxy& galaxy)
{
    constexpr float pi = 3.14159265358979323846f;

    std::mt19937 rng(galaxy.seed ^ 0x9E3779B9u);
    std::uniform_real_distribution<float> unit(0.f, 1.f);
    std::normal_distribution<float> gauss(0.f, 1.f);

    std::vector<Vec2> placed;
    placed.reserve(galaxy.systems.size());

    /** A point along one of the two arms, `t` of the way out (0 = core, 1 = rim). */
    const auto armPoint = [&](float t) -> Vec2
    {
        const float arm = unit(rng) < 0.5f ? 0.f : pi;
        const float angle = arm + t * 3.6f + gauss(rng) * 0.32f * (1.15f - t * 0.6f);
        const float radius = t * galaxyRadius + gauss(rng) * galaxyRadius * 0.045f;
        return Vec2{std::cos(angle), std::sin(angle)} * radius;
    };

    const auto candidate = [&](std::size_t index) -> Vec2
    {
        // The start sits near the outer end of an arm, among neighbours, as far from the core as
        // the galaxy allows.
        if (index == 0)
            return armPoint(0.88f);

        for (int attempt = 0; attempt < 16; ++attempt)
        {
            Vec2 position;

            // About a sixth of systems crowd the central bulge; the rest trail along the arms.
            if (unit(rng) < 0.16f)
            {
                const float radius = std::abs(gauss(rng)) * galaxyRadius * 0.13f;
                const float angle = unit(rng) * 2.f * pi;
                position = Vec2{std::cos(angle), std::sin(angle)} * radius;
            }
            else
            {
                position = armPoint(0.1f + 0.9f * std::pow(unit(rng), 0.8f));
            }

            // Re-roll anything that fell outside the disc rather than clamping it onto the rim.
            if (length(position) <= galaxyRadius)
                return position;
        }

        return armPoint(0.5f);
    };

    for (std::size_t index = 0; index < galaxy.systems.size(); ++index)
    {
        Vec2 position = candidate(index);

        for (int attempt = 0; attempt < 40; ++attempt)
        {
            const bool crowded = std::any_of(placed.begin(), placed.end(), [&](const Vec2& other)
            {
                return length(other - position) < minSystemSpacing;
            });

            if (!crowded)
                break;

            position = candidate(index);
        }

        placed.push_back(position);
        galaxy.systems[index].mapPosition = position;
    }
}

/** Chart distance between two systems, in light years. */
inline float galacticDistance(const SystemInfo& a, const SystemInfo& b)
{
    return length(a.mapPosition - b.mapPosition);
}

/** Builds a procgen: one seed in, 1000 deterministic SystemInfo entries out, laid out on the chart. */
inline Galaxy generateGalaxy(std::uint32_t seed, int systemCount = 1000)
{
    Galaxy galaxy;
    galaxy.seed = seed;
    galaxy.systems.reserve(static_cast<std::size_t>(systemCount));

    for (int systemIndex = 0; systemIndex < systemCount; ++systemIndex)
        galaxy.systems.push_back(generateSystemInfo(deriveSystemSeed(seed, systemIndex)));

    generateGalaxyLayout(galaxy);
    return galaxy;
}

#endif //DUSK_GALAXY_H
