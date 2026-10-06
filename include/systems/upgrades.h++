//
// Ship upgrades sold at stations: the catalogue, what each costs given what's already fitted, and
// buying one. Five kinds exist today: cargo bays and fuel tanks (tiered; a better one replaces a
// worse one), and the docking computer and the two scanners (one-offs). The catalogue is a table
// so more can follow.
//

#ifndef DUSK_UPGRADES_H
#define DUSK_UPGRADES_H

#include "objects/cargo.h++"
#include "objects/commander.h++"
#include "objects/fuel_tank.h++"
#include <array>
#include <cstddef>
#include <string>

/** What an upgrade is: each kind has its own slot on the ship, and tiers within it replace one another. */
enum class UpgradeKind { CargoBay, FuelTank, DockingComputer, EconomicsScanner, PoliticalScanner };

/** One thing the outfitting page can sell. */
struct ShipUpgrade {
    const char* name;
    const char* description;

    /** List price in credits. */
    double price;

    UpgradeKind kind;

    /** Which tier of its kind this is (1 = Mk1, 2 = Mk2). Higher tiers replace lower ones. */
    int tier;
};

/** The catalogue, in the order the outfitting page lists it. */
inline const std::array<ShipUpgrade, 7>& upgradeCatalogue()
{
    static const std::array<ShipUpgrade, 7> catalogue =
    {{
        {"CARGO BAY MK1", "Extra hold space: +5 t, 15 t in all.", 1800.0, UpgradeKind::CargoBay, 1},
        {"CARGO BAY MK2", "A bigger bay: +10 t, 20 t in all. Replaces Mk1.", 4200.0, UpgradeKind::CargoBay, 2},
        {"FUEL TANK MK1", "A larger tank: +4 t, 10 t in all (66 LY of jumps).", 2000.0, UpgradeKind::FuelTank, 1},
        {"FUEL TANK MK2", "A bigger tank: +8 t, 14 t in all (93 LY). Replaces Mk1.", 3500.0, UpgradeKind::FuelTank, 2},
        {"DOCKING COMPUTER", "Auto-docking on [C] for the Space Union's 15 CR fee.", 2500.0, UpgradeKind::DockingComputer, 1},
        {"ECONOMICS SCANNER", "Shows every system's exports on the galactic chart.", 1500.0, UpgradeKind::EconomicsScanner, 1},
        {"POLITICAL SCANNER", "Shows every system's development, Poor to Progressive.", 1000.0, UpgradeKind::PoliticalScanner, 1},
    }};

    return catalogue;
}

/** The tier of `kind` currently fitted: 0 for none. */
inline int installedTier(const Commander& commander, UpgradeKind kind)
{
    switch (kind)
    {
        case UpgradeKind::CargoBay: return static_cast<int>(commander.cargoModule);
        case UpgradeKind::FuelTank: return static_cast<int>(commander.fuelTank);
        case UpgradeKind::DockingComputer: return commander.hasDockingComputer ? 1 : 0;
        case UpgradeKind::EconomicsScanner: return commander.hasEconomicsScanner ? 1 : 0;
        case UpgradeKind::PoliticalScanner: return commander.hasPoliticalScanner ? 1 : 0;
    }

    return 0;
}

/** List price of tier `tier` of `kind` (tier 0, nothing fitted, is worth nothing). */
inline double upgradeValue(UpgradeKind kind, int tier)
{
    for (const ShipUpgrade& upgrade : upgradeCatalogue())
    {
        if (upgrade.kind == kind && upgrade.tier == tier)
            return upgrade.price;
    }

    return 0.0;
}

/** Fraction of its list price the outfitters give back for the module being replaced. */
constexpr double tradeInFraction = 0.5;

/** Credits the commander must keep after buying an upgrade, so there's always something left to trade with. */
constexpr double upgradeReserveCredits = 200.0;

/** Why an upgrade can or can't be bought right now. */
enum class UpgradeStatus { Available, Installed, HaveBetter, CantAfford };

/** What buying an upgrade would involve: its status, and the net cost after trading in the fitted module. */
struct UpgradeOffer {
    UpgradeStatus status = UpgradeStatus::Available;
    double cost = 0.0;
};

/**
 * Works out whether `upgrade` can be bought by `commander`. Tiers of one kind replace each other,
 * and the tier being replaced is traded in at tradeInFraction of its list price, so a Mk2 fuel
 * tank over a Mk1 costs 3500 - 1000 = 2500 credits. The commander has to be left with at least
 * upgradeReserveCredits.
 */
inline UpgradeOffer offerFor(const ShipUpgrade& upgrade, const Commander& commander)
{
    UpgradeOffer offer;
    const int fitted = installedTier(commander, upgrade.kind);

    if (upgrade.tier == fitted)
    {
        offer.status = UpgradeStatus::Installed;
        return offer;
    }

    if (upgrade.tier < fitted)
    {
        offer.status = UpgradeStatus::HaveBetter;
        return offer;
    }

    offer.cost = upgrade.price - tradeInFraction * upgradeValue(upgrade.kind, fitted);
    offer.status = commander.credits - offer.cost >= upgradeReserveCredits ? UpgradeStatus::Available : UpgradeStatus::CantAfford;
    return offer;
}

/** Fits an upgrade to the ship, replacing whatever of its kind was there. No payment: see buyUpgrade(). */
inline void installUpgrade(Commander& commander, const ShipUpgrade& upgrade)
{
    switch (upgrade.kind)
    {
        case UpgradeKind::CargoBay: commander.cargoModule = cargoModuleFromInt(upgrade.tier); break;
        case UpgradeKind::FuelTank: commander.fuelTank = fuelTankFromInt(upgrade.tier); break;
        case UpgradeKind::DockingComputer: commander.hasDockingComputer = true; break;
        case UpgradeKind::EconomicsScanner: commander.hasEconomicsScanner = true; break;
        case UpgradeKind::PoliticalScanner: commander.hasPoliticalScanner = true; break;
    }
}

/** Buys catalogue entry `index` if allowed: takes the credits and fits it. Returns the offer that applied. */
inline UpgradeOffer buyUpgrade(Commander& commander, std::size_t index)
{
    const auto& catalogue = upgradeCatalogue();

    if (index >= catalogue.size())
        return {UpgradeStatus::HaveBetter, 0.0};

    const UpgradeOffer offer = offerFor(catalogue[index], commander);

    if (offer.status == UpgradeStatus::Available)
    {
        commander.credits -= offer.cost;
        installUpgrade(commander, catalogue[index]);
    }

    return offer;
}

#endif //DUSK_UPGRADES_H
