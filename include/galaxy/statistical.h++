//
// Created by Mykyta Khomiakov on 26/07/2026.
//

#ifndef DUSK_STATISTICAL_H
#define DUSK_STATISTICAL_H

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

/** Broad economic development level of a system. */
enum class EconomyTier { Poor, Developing, Progressive };

/** Procedurally generated "on paper" facts about a system, cheap enough to hold 1000 of at once. */
struct SystemInfo {
    std::string name;
    int planetCount = 0;
    EconomyTier economyTier = EconomyTier::Poor;
    std::vector<std::string> goods;
    std::string occupation;
    int stationCount = 0;
};

/* Name generation */

/** Syllable pool used to build pronounceable procedural system names. */
inline const std::array<std::string, 20>& nameSyllables()
{
    static const std::array<std::string, 20> syllables =
    {
        "Ar", "Bel", "Cor", "Dra", "El", "Fen", "Gar", "Hyl", "Il", "Jor",
        "Kel", "Lun", "Mor", "Nyx", "Or", "Pyr", "Quel", "Ryn", "Sol", "Tara"
    };
    return syllables;
}

/** Catalog suffix styles, mixing human-designation and Roman-numeral-esque flavor. */
inline const std::array<std::string, 8>& nameSuffixes()
{
    static const std::array<std::string, 8> suffixes =
    {
        "", "", " Prime", " Minor", " Major", "-IV", "-VII", " Reach"
    };
    return suffixes;
}

/** Builds a two-or-three-syllable procedural system name from an RNG. */
inline std::string generateSystemName(std::mt19937& rng)
{
    const auto& syllables = nameSyllables();
    const auto& suffixes = nameSuffixes();

    std::uniform_int_distribution<std::size_t> syllableIndex(0, syllables.size() - 1);
    std::uniform_int_distribution<int> syllableCount(2, 3);
    std::uniform_int_distribution<std::size_t> suffixIndex(0, suffixes.size() - 1);

    std::string name;
    const int syllableTotal = syllableCount(rng);

    for (int i = 0; i < syllableTotal; ++i)
        name += syllables[syllableIndex(rng)];

    name += suffixes[suffixIndex(rng)];
    return name;
}

/* Economy and occupation */

/** Rolls an economy tier, weighted so poor systems are the most common. */
inline EconomyTier generateEconomyTier(std::mt19937& rng)
{
    std::discrete_distribution<int> distribution({50, 35, 15}); // Poor, Developing, Progressive
    return static_cast<EconomyTier>(distribution(rng));
}

/** Occupation pool, matching the roadmap's world-occupation categories. */
inline const std::array<std::string, 3>& occupationPool()
{
    static const std::array<std::string, 3> occupations =
    {
        "Mining", "Engineering and Tech", "Agricultural"
    };
    return occupations;
}

/** Rolls a dominant system occupation. */
inline std::string generateOccupation(std::mt19937& rng)
{
    const auto& occupations = occupationPool();
    std::uniform_int_distribution<std::size_t> index(0, occupations.size() - 1);
    return occupations[index(rng)];
}

/* Goods */

/** Full tradeable-goods pool, matching the roadmap's goods list. */
inline const std::array<std::string, 11>& allGoods()
{
    static const std::array<std::string, 11> goods =
    {
        "Silicon chips", "Food", "Liquor", "Wines", "Base ores",
        "Advanced ores", "Advanced electronics", "Furs", "Animals",
        "Books", "Chemical fuel"
    };
    return goods;
}

/** Goods an occupation is likely to produce, biasing which goods a system sells. */
inline std::vector<std::string> goodsForOccupation(const std::string& occupation)
{
    if (occupation == "Mining")
        return {"Base ores", "Advanced ores", "Chemical fuel"};

    if (occupation == "Engineering and tech")
        return {"Silicon chips", "Advanced electronics", "Books"};

    return {"Food", "Liquor", "Wines", "Furs", "Animals"};
}

/** Rolls 2-4 goods a system best sells, favoring its occupation but occasionally reaching outside it. */
inline std::vector<std::string> generateGoods(std::mt19937& rng, const std::string& occupation)
{
    const std::vector<std::string> favored = goodsForOccupation(occupation);
    const auto& everything = allGoods();

    std::uniform_int_distribution<int> goodsCount(2, 4);
    std::uniform_real_distribution<float> outsideChance(0.f, 1.f);
    std::uniform_int_distribution<std::size_t> favoredIndex(0, favored.size() - 1);
    std::uniform_int_distribution<std::size_t> anyIndex(0, everything.size() - 1);

    std::vector<std::string> goods;
    const int total = goodsCount(rng);

    while (static_cast<int>(goods.size()) < total)
    {
        const std::string pick = outsideChance(rng) < 0.2f
            ? everything[anyIndex(rng)]
            : favored[favoredIndex(rng)];

        if (std::find(goods.begin(), goods.end(), pick) == goods.end())
            goods.push_back(pick);
    }

    return goods;
}

/* Planets and stations */

/** Rolls how many planetary objects a system contains. */
inline int generatePlanetCount(std::mt19937& rng)
{
    std::uniform_int_distribution<int> distribution(1, 8);
    return distribution(rng);
}

/** Rolls how many stations a system has, weighted toward one or zero. */
inline int generateStationCount(std::mt19937& rng, EconomyTier economyTier)
{
    // More developed systems are more likely to host a station.
    const int bonus = economyTier == EconomyTier::Progressive
        ? 1
        : (economyTier == EconomyTier::Developing ? 0 : -1);

    std::discrete_distribution<int> distribution({40, 45, 15}); // 0, 1, 2 stations, before bonus
    const int rolled = distribution(rng);
    return std::clamp(rolled + (bonus > 0 ? 1 : 0) - (bonus < 0 ? 1 : 0), 0, 3);
}

/* Top-level generation */

/** Pure function: the same seed always produces the same SystemInfo. */
inline SystemInfo generateSystemInfo(std::uint32_t systemSeed)
{
    std::mt19937 rng(systemSeed);
    SystemInfo info;

    info.name = generateSystemName(rng);
    info.economyTier = generateEconomyTier(rng);
    info.occupation = generateOccupation(rng);
    info.goods = generateGoods(rng, info.occupation);
    info.planetCount = generatePlanetCount(rng);
    info.stationCount = generateStationCount(rng, info.economyTier);

    return info;
}

/** Derives a stable per-system seed from the galaxy seed and system index. */
inline std::uint32_t deriveSystemSeed(std::uint32_t gameSeed, int systemIndex)
{
    std::seed_seq seedSequence{gameSeed, static_cast<std::uint32_t>(systemIndex)};
    std::array<std::uint32_t, 1> output{};
    seedSequence.generate(output.begin(), output.end());
    return output[0];
}

/** Generates the full roster of on-paper systems for a galaxy seed. */
inline std::vector<SystemInfo> generateGalaxy(std::uint32_t gameSeed, int systemCount = 1000)
{
    std::vector<SystemInfo> systems;
    systems.reserve(static_cast<std::size_t>(systemCount));

    for (int systemIndex = 0; systemIndex < systemCount; ++systemIndex)
        systems.push_back(generateSystemInfo(deriveSystemSeed(gameSeed, systemIndex)));

    return systems;
}

#endif //DUSK_STATISTICAL_H