//
// What docking costs. Every docking pays the Space Union a fee. A ship with a docking computer
// pays the standard fee; a ship without one has to be flown in by a Union tug, which costs far more.
// (There is no manual docking yet, so without the tug a ship that left a station could never
// dock again, and a new commander can't afford the computer yet.)
//

#ifndef DUSK_DOCKING_FEES_H
#define DUSK_DOCKING_FEES_H

#include "objects/commander.h++"
#include <algorithm>

/** The Space Union's docking fee for a ship with a docking computer, in credits. */
constexpr double dockingFee = 15.0;

/**
 * The charge for a ship without a docking computer, which the Union flies in on a tug. Chosen so
 * that a new commander needs about six good trades to afford the computer (and the computer
 * then saves them this minus dockingFee on every docking).
 */
constexpr double unionTugFee = 200.0;

/** What `commander` owes for the next docking. */
inline double dockingChargeFor(const Commander& commander)
{
    return commander.hasDockingComputer ? dockingFee : unionTugFee;
}

/** What was charged for a docking. */
struct DockingCharge {
    double due = 0.0;     // what the fee was
    double paid = 0.0;    // what was actually taken
};

/**
 * Takes the docking fee from the commander. The Union takes at most what you have: a commander
 * with an empty purse and a hold of goods to sell must still be able to dock, or they could never
 * earn their way out. `paid` is less than `due` when the commander couldn't cover it.
 */
inline DockingCharge chargeForDocking(Commander& commander)
{
    DockingCharge charge;
    charge.due = dockingChargeFor(commander);
    charge.paid = std::min(charge.due, std::max(0.0, commander.credits));
    commander.credits -= charge.paid;
    return charge;
}

#endif //DUSK_DOCKING_FEES_H
