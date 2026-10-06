//
// The player's persistent state that isn't part of the ship's physics: name and credits.
// SystemScene owns one Commander and keeps it across hyperspace jumps.
//

#ifndef DUSK_COMMANDER_H
#define DUSK_COMMANDER_H

#include "objects/cargo.h++"
#include <algorithm>
#include <string>

/**
 * The player, Elite style: a commander with a name and a credit balance. Fuel lives on the Ship
 * (it has mass); money, and later reputation, missions and owned ships, live here.
 */
struct Commander {
    /** A nod to Elite's default commander. */
    std::string name = "JAMES";

    /** Credit balance. Trading earns it; missions will too. */
    double credits = 1000.0;

    /** What the hold carries, and the cargo bay module fitted to it. */
    CargoHold cargo;
    CargoModule cargoModule = CargoModule::None;

    /** Hold space in tonnes: 10 standard, 15 with a Mk1 bay, 20 with a Mk2 bay. */
    int cargoCapacity() const
    {
        return cargoCapacityFor(cargoModule);
    }

    /** Free hold space in tonnes. */
    int cargoRoom() const
    {
        return std::max(0, cargoCapacity() - cargo.total());
    }
};

#endif //DUSK_COMMANDER_H
