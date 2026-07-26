//
// Created by Mykyta Khomiakov on 26/07/2026.
//

#ifndef DUSK_ECONOMY_H
#define DUSK_ECONOMY_H

#include "procgen/statistical.h++"
#include <string>
#include <unordered_map>
#include <vector>

/** A good's name and its price in a specific system, before any player trading has occurred. */
struct GoodPrice {
    std::string name;
    int price = 0;
};

/** Baseline galaxy-wide price for each tradeable good, before system modifiers. */
inline int basePriceFor(const std::string& good)
{
    static const std::unordered_map<std::string, int> basePrices =
    {
        {"Silicon chips", 220}, {"Food", 40}, {"Liquor", 90}, {"Wines", 140},
        {"Base ores", 60}, {"Advanced ores", 260}, {"Advanced electronics", 380},
        {"Furs", 120}, {"Animals", 95}, {"Books", 55}, {"Chemical fuel", 75}
    };

    const auto found = basePrices.find(good);
    return found != basePrices.end() ? found->second : 100;
}

/** Poorer systems pay more for everything; progressive systems undercut the base price. */
inline float economyTierMultiplier(EconomyTier tier)
{
    switch (tier)
    {
        case EconomyTier::Poor:        return 1.15f;
        case EconomyTier::Developing:  return 1.f;
        case EconomyTier::Progressive: return 0.85f;
    }

    return 1.f;
}

/**
 * Computes on-paper sell prices for a system's best-sold goods. Pure function of an
 * already-generated SystemInfo, so it's cheap to call whenever the map/station UI needs
 * it instead of caching it. It deliberately knows nothing about player-driven supply and
 * demand — that belongs in a separate, mutable per-system runtime state.
 */
inline std::vector<GoodPrice> computeSystemPrices(const SystemInfo& info)
{
    std::vector<GoodPrice> prices;
    prices.reserve(info.goods.size());

    const float multiplier = economyTierMultiplier(info.economyTier);

    for (const std::string& good : info.goods)
    {
        // A system's specialty goods sell at a local surplus discount.
        const int price = static_cast<int>(static_cast<float>(basePriceFor(good)) * multiplier * 0.85f);
        prices.push_back({good, price});
    }

    return prices;
}

#endif //DUSK_ECONOMY_H