//
// What docking costs, and the spending reserve that makes sure it can always be paid. Every
// docking pays the Space Union a fee. A ship with a docking computer
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

/**
 * The credits every purchase must leave you with: what your next docking will cost (200 CR on the
 * Union tug, 15 CR with a docking computer). Nothing can be bought, whether goods, fuel or an
 * upgrade, if it would leave you with less, so you always arrive able to pay the fee. Tying it to
 * the actual charge means it tracks the fees, and a commander who owns the computer isn't made to
 * hold back 200 CR to cover a 15 CR fee.
 */
inline double spendingReserve(const Commander& commander)
{
    return dockingChargeFor(commander);
}

/** Credits that can be spent right now: everything above the reserve (never negative). */
inline double spendableCredits(const Commander& commander)
{
    return std::max(0.0, commander.credits - spendingReserve(commander));
}

/** True if a purchase costing `cost` is refused *only* because of the reserve: you hold enough, but not enough to keep it. */
inline bool reserveBlocks(const Commander& commander, double cost)
{
    return commander.credits >= cost && commander.credits - cost < spendingReserve(commander);
}

/** What was charged for a docking. */
struct DockingCharge {
    double due = 0.0;     // what the fee was
    double paid = 0.0;    // what was actually taken
};

/**
 * Takes the docking fee from the commander. The Union takes at most what you have, as a last
 * safety net: a commander with an empty purse and a hold of goods to sell must still be able to
 * dock, or they could never earn their way out. Because purchases keep the spending reserve, that
 * only happens to a commander who has fallen below it through fees or losses, never one who spent
 * their credits (which would otherwise be a way to dock for free). `paid` is less than `due` when
 * the commander couldn't cover it.
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
