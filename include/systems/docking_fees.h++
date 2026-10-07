//
// What docking costs, and the spending reserve that makes sure it can always be paid. Every docking
// pays the Space Union a fee, however it was done (by hand or with the docking computer), and a hull
// contact with the station is fined.
//

#ifndef DUSK_DOCKING_FEES_H
#define DUSK_DOCKING_FEES_H

#include "objects/commander.h++"
#include <algorithm>

/** The Space Union's docking fee, in credits, paid when a docking completes. */
constexpr double dockingFee = 15.0;

/** The fine for scraping the station, in credits, taken only from credits above the spending reserve. */
constexpr double collisionFine = 25.0;

/** What `commander` owes for the next docking. (It's the same for everyone now that docking can be done by hand.) */
inline double dockingChargeFor(const Commander&)
{
    return dockingFee;
}

/**
 * The credits every purchase must leave you with: what your next docking will cost. Nothing can be
 * bought, whether goods, fuel or an upgrade, if it would leave you with less, so you always arrive
 * able to pay the fee, and can't dodge it by spending everything first.
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
 * only happens to a commander who has fallen below it through fines or trading losses, never one who
 * spent their credits. `paid` is less than `due` when the commander couldn't cover it.
 */
inline DockingCharge chargeForDocking(Commander& commander)
{
    DockingCharge charge;
    charge.due = dockingChargeFor(commander);
    charge.paid = std::min(charge.due, std::max(0.0, commander.credits));
    commander.credits -= charge.paid;
    return charge;
}

/**
 * Fines the commander for a hull contact. The fine comes only out of spendable credits (those above
 * the reserve), so it can never leave the commander unable to pay the docking fee. Returns what was
 * actually taken, which is less than collisionFine, or nothing, for a commander with little to spare.
 */
inline double fineForContact(Commander& commander)
{
    const double fine = std::min(collisionFine, spendableCredits(commander));
    commander.credits -= fine;
    return fine;
}

#endif //DUSK_DOCKING_FEES_H
