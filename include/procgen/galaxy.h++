//
// Created by Mykyta Khomiakov on 26/07/2026.
//

#ifndef DUSK_GALAXY_H
#define DUSK_GALAXY_H

#include "statistical.h++"
#include <array>
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

/** Builds a procgen: one seed in, 1000 deterministic SystemInfo entries out. */
inline Galaxy generateGalaxy(std::uint32_t seed, int systemCount = 1000)
{
    Galaxy galaxy;
    galaxy.seed = seed;
    galaxy.systems.reserve(static_cast<std::size_t>(systemCount));

    for (int systemIndex = 0; systemIndex < systemCount; ++systemIndex)
        galaxy.systems.push_back(generateSystemInfo(deriveSystemSeed(seed, systemIndex)));

    return galaxy;
}

#endif //DUSK_GALAXY_H
