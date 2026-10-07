//
// Station refuelling: fuel prices per system, and buying fuel with credits.
// Pure functions over a Ship and a Commander, so the station menu only has to display and call them.
//

#ifndef DUSK_REFUELLING_H
#define DUSK_REFUELLING_H

#include "objects/commander.h++"
#include "objects/ship.h++"
#include "systems/docking_fees.h++"
#include "procgen/statistical.h++"
#include "systems/economy.h++"
#include <algorithm>
#include <cmath>

/** Galaxy-wide base fuel price in credits per tonne, before the system's economy adjusts it. */
constexpr float baseFuelPricePerTonne = 15.f;

/** Fuel price in a system: dearer in poor systems, cheaper in progressive ones (economyTierMultiplier()). */
inline float fuelPricePerTonne(const SystemInfo& info)
{
    return baseFuelPricePerTonne * economyTierMultiplier(info.economyTier);
}

/** What a refuel would deliver and cost. */
struct RefuelQuote {
    float tonnes = 0.f;
    double cost = 0.0;

    /** True when the tank is already full, so there is nothing to buy. */
    bool tankFull = false;

    /** True when the quote was cut short because the commander can't afford the full amount. */
    bool limitedByCredits = false;
};

/**
 * Quotes buying up to `wantedTonnes` of fuel: never more than the tank has room for, nor more than
 * the commander can pay for. Fuel is sold in 0.1 t steps, Elite style, and priced per step.
 */
inline RefuelQuote quoteRefuel(const Ship& ship, const Commander& commander, float pricePerTonne, float wantedTonnes)
{
    RefuelQuote quote;
    const float room = std::max(0.f, ship.fuelCapacity - ship.fuel);

    if (room < 0.05f)
    {
        quote.tankFull = true;
        return quote;
    }

    constexpr float step = 0.1f;
    float tonnes = std::min(wantedTonnes, room);

    // Round up to whole steps when filling the tank, so a nearly full tank still tops off.
    if (tonnes >= room - 1e-4f)
        tonnes = room;
    else
        tonnes = std::floor(tonnes / step + 1e-4f) * step;

    if (pricePerTonne > 0.f)
    {
        // Fuel, like everything else, can't take you below the spending reserve.
        const float affordable = static_cast<float>(spendableCredits(commander) / pricePerTonne);

        if (affordable < tonnes)
        {
            tonnes = std::floor(affordable / step + 1e-4f) * step;
            quote.limitedByCredits = true;
        }
    }

    quote.tonnes = std::max(0.f, tonnes);
    quote.cost = static_cast<double>(quote.tonnes) * pricePerTonne;
    return quote;
}

/** Buys fuel as quoted: moves it into the tank and takes the credits. Returns what was bought. */
inline RefuelQuote buyFuel(Ship& ship, Commander& commander, float pricePerTonne, float wantedTonnes)
{
    const RefuelQuote quote = quoteRefuel(ship, commander, pricePerTonne, wantedTonnes);

    if (quote.tonnes <= 0.f)
        return quote;

    ship.fuel = std::min(ship.fuelCapacity, ship.fuel + quote.tonnes);
    commander.credits = std::max(0.0, commander.credits - quote.cost);
    return quote;
}

#endif //DUSK_REFUELLING_H
