//
// Cargo: the goods the game trades in, the ship's hold that carries them, and the cargo bay
// modules that enlarge it. Pure data and arithmetic; prices and markets live in systems/trading.h++.
//

#ifndef DUSK_CARGO_H
#define DUSK_CARGO_H

#include "procgen/statistical.h++"
#include <algorithm>
#include <array>
#include <string>

/** How many different goods exist (the size of allGoods()). */
constexpr int goodCount = 11;

/** Index of a good by name (its position in allGoods()), or -1 if there's no such good. */
inline int goodIndex(const std::string& name)
{
    const auto& goods = allGoods();

    for (int index = 0; index < goodCount; ++index)
    {
        if (goods[static_cast<std::size_t>(index)] == name)
            return index;
    }

    return -1;
}

/** Name of good `index`; an empty string for an index out of range. */
inline const std::string& goodName(int index)
{
    static const std::string none;
    return index >= 0 && index < goodCount ? allGoods()[static_cast<std::size_t>(index)] : none;
}

/**
 * What's in the ship's hold: whole tonnes of each good. How much the hold can carry depends on
 * the installed cargo module (see cargoCapacityFor()), so capacity is passed in where it matters.
 */
struct CargoHold {
    std::array<int, goodCount> tonnes{};

    /** Total tonnes carried, of everything. */
    int total() const
    {
        int sum = 0;

        for (const int amount : tonnes)
            sum += amount;

        return sum;
    }

    /** Tonnes of one good (0 for an invalid index). */
    int of(int good) const
    {
        return good >= 0 && good < goodCount ? tonnes[static_cast<std::size_t>(good)] : 0;
    }

    /** Adds tonnes of a good (never below zero). */
    void add(int good, int amount)
    {
        if (good >= 0 && good < goodCount)
            tonnes[static_cast<std::size_t>(good)] = std::max(0, tonnes[static_cast<std::size_t>(good)] + amount);
    }

    /**
     * Brings the hold within `capacity` tonnes by dropping goods from the last index backwards.
     * Only used to tidy a corrupt save; in play the hold is never over capacity.
     */
    void trimTo(int capacity)
    {
        for (int good = goodCount - 1; good >= 0 && total() > capacity; --good)
        {
            const int excess = total() - capacity;
            const int drop = std::min(excess, tonnes[static_cast<std::size_t>(good)]);
            tonnes[static_cast<std::size_t>(good)] -= drop;
        }
    }
};

/**
 * The cargo bay module fitted to the ship. Modules replace one another rather than stacking: a
 * Mk2 bay is a bigger bay, not a second one.
 */
enum class CargoModule { None = 0, Mk1 = 1, Mk2 = 2 };

/** Hold space of the standard ship, with no module fitted, in tonnes. */
constexpr int baseCargoCapacity = 10;

/** Extra hold space a module adds: Mk1 +5 t, Mk2 +10 t. */
inline int cargoModuleBonus(CargoModule module)
{
    switch (module)
    {
        case CargoModule::None: return 0;
        case CargoModule::Mk1: return 5;
        case CargoModule::Mk2: return 10;
    }

    return 0;
}

/** Total hold space with a module fitted: 10 t standard, 15 t with Mk1, 20 t with Mk2. */
inline int cargoCapacityFor(CargoModule module)
{
    return baseCargoCapacity + cargoModuleBonus(module);
}

/** Display name of a module ("STANDARD HOLD", "CARGO BAY MK1", ...). */
inline const char* cargoModuleName(CargoModule module)
{
    switch (module)
    {
        case CargoModule::None: return "STANDARD HOLD";
        case CargoModule::Mk1: return "CARGO BAY MK1";
        case CargoModule::Mk2: return "CARGO BAY MK2";
    }

    return "";
}

/** Converts a saved integer to a module, treating anything unknown as no module. */
inline CargoModule cargoModuleFromInt(int value)
{
    return value >= 2 ? CargoModule::Mk2 : (value == 1 ? CargoModule::Mk1 : CargoModule::None);
}

#endif //DUSK_CARGO_H
