//
// Created by Mykyta Khomiakov on 26/07/2026.
//
// NPC ships: a regular Ship body with a small simple-reflex state machine on top
// (inactive, warping in, roaming, docked, warping out). Behaviour lives in systems/npc_ai.h++.
//

#ifndef DUSK_NPC_SHIP_H
#define DUSK_NPC_SHIP_H

#include "math/Vec3.h++"
#include "objects/ship.h++"

/** Coarse behavioural state for a simple-reflex NPC ship. */
enum class NpcState {
    Inactive,          // warped out, invisible, waiting to respawn
    Roaming,           // flying toward a roam waypoint, avoiding hazards
    HeadingToStation,  // flying to the approach point out in front of the station's docking slot
    EnteringStation,   // lined up on the slot, flying down its axis into the station
    Docked,            // inside the station, invisible
    Launching,         // flying out of the slot along its axis, before roaming again
    WarpingOut         // brief wind-up before vanishing
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

    /**
     * Entering / Launching: distance of the ship from the slot's mouth along the slot normal
     * (positive outside the station, negative inside), and its sideways offset from the slot
     * axis, which is eased to zero as it lines up.
     */
    float dockDistance = 0.f;
    Vec3 dockLateral;

    /** Whether this NPC should currently be rendered: everywhere except warped out or inside the station. */
    bool isVisible() const
    {
        return state != NpcState::Inactive && state != NpcState::Docked;
    }
};

#endif //DUSK_NPC_SHIP_H