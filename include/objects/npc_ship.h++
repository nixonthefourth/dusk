//
// Created by Mykyta Khomiakov on 26/07/2026.
//
// NPC ships: a regular Ship body with a small simple-reflex state machine on top
// (inactive, warping in, roaming, docked, warping out). Behaviour lives in systems/npc_ai.h++.
//

#ifndef DUSK_NPC_SHIP_H
#define DUSK_NPC_SHIP_H

#include <cstdint>
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
    WarpingOut,        // brief wind-up before vanishing
    Hostile            // a pirate hunting the player: flown by systems/piracy.h++, not by the NPC AI
};

/** One simple-reflex NPC ship: a Ship body plus AI state layered on top. */
/** NPC ships' shield and hull points when undamaged. Shields take damage first and recharge; hull doesn't. */
constexpr float npcMaxShields = 40.f;
constexpr float npcMaxHull = 60.f;

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

    /** Combat: shield and hull points (the ship is destroyed at 0 hull), and seconds since it was last hit. */
    float shields = npcMaxShields;
    float hull = npcMaxHull;
    float sinceHit = 1e9f;

    /** True once its shields have failed under fire and it is running for it. */
    bool fleeing = false;

    /* ---- Pirates (see systems/piracy.h++) ---------------------------------------------------- */

    /** A pirate: it hunts the player instead of going about its business, and pays a bounty when destroyed. */
    bool hostile = false;

    /** A pirate that has left play for good (destroyed, escaped or gone); its slot can be reused for the next pirate. */
    bool retired = false;

    /** Never runs, however damaged (bounty targets fight to the end). */
    bool fearless = false;

    /** The bounty mission this pirate is a target of, or 0. */
    std::uint64_t missionId = 0;

    /** Seconds until it can fire again, which of its two guns fires next, seconds spent running, and which way it circles. */
    float fireCooldown = 0.f;
    int nextGun = 0;
    float fleeTimer = 0.f;
    float circleSign = 1.f;

    /** Whether this NPC should currently be rendered: everywhere except warped out or inside the station. */
    bool isVisible() const
    {
        return state != NpcState::Inactive && state != NpcState::Docked;
    }
};

#endif //DUSK_NPC_SHIP_H