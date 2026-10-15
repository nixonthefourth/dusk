//
// What combat puts in the world: laser bolts and rockets in flight, smoke, sparks and debris,
// explosions, shield flashes, and the state of the beam laser, the turret and the weapon capacitor.
//
// This is plain data. systems/combat.h++ moves it and decides what hits what, and
// rendering/combat_renderer.h++ draws it. It lives in the World, so a jump clears it.
//

#ifndef DUSK_COMBAT_STATE_H
#define DUSK_COMBAT_STATE_H

#include "math/Vec3.h++"
#include <cstdint>
#include <vector>

namespace combat {

    /** Capacity of the weapon capacitor, which the lasers draw on. */
    constexpr float energyCapacity = 100.f;

    /** A laser bolt in flight (from the pulse laser or the turret). */
    struct Bolt {
        Vec3 position;
        Vec3 velocity;
        float life = 0.f;        // seconds left before it fades out
        float damage = 0.f;
        bool fromTurret = false; // drawn in the turret's colour
        bool hostile = false;    // fired by a pirate: it hits the player, never other ships
    };

    /** The player's ship's shield and hull points, when undamaged. */
    constexpr float playerMaxShields = 60.f;
    constexpr float playerMaxHull = 100.f;

    /** How the player's ship is holding up. */
    struct PlayerHealth {
        float shields = playerMaxShields;
        float hull = playerMaxHull;

        /** Seconds since it was last hit (shields recharge after a pause), and since its hull was last hit (for the HUD's red flash). */
        float sinceHit = 1e9f;
        float sinceHullHit = 1e9f;

        /** True once the hull has reached zero: the ship has been destroyed. */
        bool destroyed = false;

        /** What destroyed it, for the insurance message ("SHOT DOWN", "BURNED UP IN THE ATMOSPHERE"...). */
        const char* cause = "";
    };

    /** HitFlash::ship value meaning the player's own ship. */
    constexpr int playerShip = -2;

    /** A rocket in flight. */
    struct Rocket {
        Vec3 position;
        Vec3 velocity;
        float life = 0.f;        // seconds of fuel left; it detonates when it runs out
        float topSpeed = 0.f;    // the speed it accelerates to (its launch speed plus its own)
        int targetShip = -1;     // index of the NPC ship it homes on, or -1 to fly straight
        float smokeTimer = 0.f;
    };

    /** What a particle is, which decides how it looks and moves. */
    enum class ParticleKind { Smoke, Spark, Debris, Flash };

    /** A short-lived particle: rocket smoke, sparks from a hit, debris from an explosion, a muzzle flash. */
    struct Particle {
        ParticleKind kind = ParticleKind::Spark;
        Vec3 position;
        Vec3 velocity;
        float age = 0.f;
        float life = 1.f;
        float size = 1.f;
        float spin = 0.f;        // debris tumbles
    };

    /** An explosion: a flash and a shock ring that grow and fade. */
    struct Explosion {
        Vec3 position;
        Vec3 velocity;
        float age = 0.f;
        float life = 1.f;
        float radius = 200.f;    // how far the shock ring spreads, in world units
    };

    /** A hit on a ship: a flash of its shield bubble, or (shields down) sparks off its hull. */
    struct HitFlash {
        int ship = -1;           // NPC index, followed while the flash lasts
        Vec3 offset;             // where on the ship, relative to its centre
        float age = 0.f;
        bool shield = true;
    };

    /** What a fired weapon could be doing, for the HUD. */
    enum class WeaponStatus { Ready, Firing, LowEnergy, NoAmmo, OutOfArc };

    /** Everything combat keeps between steps. */
    struct CombatState {
        /** The weapon capacitor: energy left, drawn by the lasers and recharged continuously. */
        float energy = energyCapacity;

        /** Seconds until each weapon may fire again, and which muzzle of a pair fires next. */
        float pulseCooldown = 0.f;
        float rocketCooldown = 0.f;
        float turretCooldown = 0.f;
        int nextPulseMuzzle = 0;
        int nextRocketPod = 0;

        std::vector<Bolt> bolts;
        std::vector<Rocket> rockets;
        std::vector<Particle> particles;
        std::vector<Explosion> explosions;
        std::vector<HitFlash> flashes;

        /** The beam laser this step: how bright it is (it ramps), and where it ends. */
        float beamPower = 0.f;
        bool beamHitting = false;
        Vec3 beamEnd;

        /** Set by fireWeapon() while the beam is fired this step; otherwise updateCombat() fades it. */
        bool beamHeld = false;

        /** Where the turret points (a world-space unit vector), and whether its target is within its arc. */
        Vec3 turretAim{0.f, 0.f, 1.f};
        bool turretInitialised = false;
        bool turretInArc = false;

        /** Whether the turret's aim is close enough to its firing solution to fire. */
        bool turretOnTarget = true;

        /** Whether the player's ship has the turret (so the renderer draws it). Set by the scene. */
        bool turretFitted = false;

        /** The player's ship's shields and hull. The scene copies the hull to and from the Commander. */
        PlayerHealth player;

        /** Bounty earned for pirates destroyed since the scene last collected it, and the bounty missions those pirates belonged to. */
        double bountyEarned = 0.0;
        std::vector<std::uint64_t> missionKills;

        /** Damage pirate fire did to the station since the scene last collected it (a raid, for a defence mission). */
        float stationDamage = 0.f;

        /** What the selected weapon did this step, for the readout. */
        WeaponStatus status = WeaponStatus::Ready;

        /** State of the small random generator for effects (deterministic, for tests). */
        std::uint32_t random = 0x9E3779B9u;
    };

} // namespace combat

#endif //DUSK_COMBAT_STATE_H
