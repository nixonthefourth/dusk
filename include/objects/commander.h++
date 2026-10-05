//
// The player's persistent state that isn't part of the ship's physics: name and credits.
// SystemScene owns one Commander and keeps it across hyperspace jumps.
//

#ifndef DUSK_COMMANDER_H
#define DUSK_COMMANDER_H

#include <string>

/**
 * The player, Elite style: a commander with a name and a credit balance. Fuel lives on the Ship
 * (it has mass); money, and later reputation, missions and owned ships, live here.
 */
struct Commander {
    /** A nod to Elite's default commander. */
    std::string name = "JAMESON";

    /** Credit balance. There is no way to earn credits yet; missions will add one. */
    double credits = 1000.0;
};

#endif //DUSK_COMMANDER_H
