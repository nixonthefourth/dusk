//
// State for the cruise and hyperspace travel animations, handed from a scene to the renderer.
//

#ifndef DUSK_TRAVEL_EFFECTS_H
#define DUSK_TRAVEL_EFFECTS_H

/**
 * Stages of a hyperspace jump between systems.
 *
 *  - Countdown: an Elite-style "HYPERSPACE" countdown; you can still fly, and Escape aborts.
 *  - Accelerate: the stars stretch into streaks racing out from the nose (the Star Wars moment).
 *  - Tunnel: hyperspace itself, Elite's expanding rings with streaks rushing past. The destination
 *    system is swapped in, unseen, partway through.
 *  - Arrive: a flash, and the streaks snap back into stars around the new system.
 */
enum class HyperspacePhase { None, Countdown, Accelerate, Tunnel, Arrive };

/** Everything the travel-effects renderer needs to know that isn't already in the World. */
struct TravelEffects {
    HyperspacePhase hyperspacePhase = HyperspacePhase::None;

    /**
     * Progress through the current hyperspace phase, 0 to 1, and seconds into it. During the
     * countdown, progress instead measures the final gathering (0 until its last 1.5 seconds).
     */
    float phaseProgress = 0.f;
    float phaseTime = 0.f;

    /** Fades from 1 to 0 just after cruise engages (the outward burst) or drops out (the flash). */
    float cruiseEngageBurst = 0.f;
    float cruiseDropFlash = 0.f;
};

#endif //DUSK_TRAVEL_EFFECTS_H
