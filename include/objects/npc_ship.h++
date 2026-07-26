//
// Created by Mykyta Khomiakov on 26/07/2026.
//

#ifndef DUSK_NPC_SHIP_H
#define DUSK_NPC_SHIP_H

#include "math/Vec3.h++"
#include "objects/ship.h++"

/** Coarse behavioural state for a simple-reflex NPC ship. */
enum class NpcState {
    Inactive,        // warped out, invisible, waiting to respawn
    Roaming,         // flying toward a roam waypoint, avoiding hazards
    HeadingToStation,
    Docked,          // paused at the station, invisible
    WarpingOut       // brief wind-up before vanishing
};

/** One simple-reflex NPC ship: a Ship body plus AI state layered on top. */
struct NpcShip {
    /** Reuses the player's Ship type, so it gets the same model, physics, and renderer for free. */
    Ship ship;

    NpcState state = NpcState::Inactive;
    float stateTimer = 0.f;

    /** Inactive: how long until this ship respawns. */
    float wakeDelay = 0.f;

    /** Docked: how long it stays at the station before departing again. */
    float dockDuration = 0.f;

    /** Current roam destination while in the Roaming state. */
    Vec3 roamTarget;

    /** Whether this NPC should currently be rendered. */
    bool isVisible() const
    {
        return
            state == NpcState::Roaming ||
            state == NpcState::HeadingToStation ||
            state == NpcState::WarpingOut;
    }
};

#endif //DUSK_NPC_SHIP_H