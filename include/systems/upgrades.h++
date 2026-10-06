//
// Ship upgrades sold at stations: the catalogue, what each costs given what's already fitted, and
// buying one. Only cargo bays exist today; the catalogue is a table so more can follow.
//

#ifndef DUSK_UPGRADES_H
#define DUSK_UPGRADES_H

#include "objects/cargo.h++"
#include "objects/commander.h++"
#include <array>
#include <string>

/** One thing the outfitting page can sell. */
struct ShipUpgrade {
    const char* name;
    const char* description;

    /** List price in credits. */
    double price;

    /** The cargo bay module this upgrade installs. */
    CargoModule module;
};

/** The catalogue, in the order the outfitting page lists it. */
inline const std::array<ShipUpgrade, 2>& upgradeCatalogue()
{
    static const std::array<ShipUpgrade, 2> catalogue =
    {{
        {"CARGO BAY MK1", "Extra hold space: +5 t, 15 t in all.", 1800.0, CargoModule::Mk1},
        {"CARGO BAY MK2", "A bigger bay: +10 t, 20 t in all. Replaces Mk1.", 4200.0, CargoModule::Mk2},
    }};

    return catalogue;
}

/** List price of the module currently fitted (nothing is worth nothing). */
inline double fittedModuleValue(CargoModule fitted)
{
    for (const ShipUpgrade& upgrade : upgradeCatalogue())
    {
        if (upgrade.module == fitted)
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
 * Works out whether `upgrade` can be bought by a commander with `fitted` installed and `credits`
 * to spend. Modules replace each other, and the module being replaced is traded in at
 * tradeInFraction of its list price, so upgrading from Mk1 to Mk2 costs 4200 - 900 = 3300 credits.
 * The commander has to be left with at least upgradeReserveCredits.
 */
inline UpgradeOffer offerFor(const ShipUpgrade& upgrade, CargoModule fitted, double credits)
{
    UpgradeOffer offer;

    if (upgrade.module == fitted)
    {
        offer.status = UpgradeStatus::Installed;
        return offer;
    }

    if (static_cast<int>(upgrade.module) < static_cast<int>(fitted))
    {
        offer.status = UpgradeStatus::HaveBetter;
        return offer;
    }

    offer.cost = upgrade.price - tradeInFraction * fittedModuleValue(fitted);
    offer.status = credits - offer.cost >= upgradeReserveCredits ? UpgradeStatus::Available : UpgradeStatus::CantAfford;
    return offer;
}

/** Buys catalogue entry `index` if allowed: takes the credits and fits the module. Returns the offer that applied. */
inline UpgradeOffer buyUpgrade(Commander& commander, std::size_t index)
{
    const auto& catalogue = upgradeCatalogue();

    if (index >= catalogue.size())
        return {UpgradeStatus::HaveBetter, 0.0};

    const UpgradeOffer offer = offerFor(catalogue[index], commander.cargoModule, commander.credits);

    if (offer.status == UpgradeStatus::Available)
    {
        commander.credits -= offer.cost;
        commander.cargoModule = catalogue[index].module;
    }

    return offer;
}

#endif //DUSK_UPGRADES_H
