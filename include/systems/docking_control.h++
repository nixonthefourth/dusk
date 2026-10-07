//
// Docking control: the station's side of docking, and the numbers a pilot needs to dock by hand.
//
// Docking starts with a request. Within range, the pilot asks the station for a docking slot; the
// station answers after a short delay, queueing the request while another ship is entering or
// leaving. A granted permit lasts a few minutes. While a permit is held and the ship is near the
// station, the ship is in *docking mode*: speeds are measured against the station, throttle is
// capped by a limit that tightens as the slot nears, the ship's roll follows the slot's spin, and
// the guidance panel and corridor show how well it lines up.
//
// Everything here is plain logic over the World and the ship, with no drawing, so it can be tested
// headless.
//

#ifndef DUSK_DOCKING_CONTROL_H
#define DUSK_DOCKING_CONTROL_H

#include "objects/cube.h++"
#include "objects/ship.h++"
#include "systems/docking_computer.h++"
#include "world/world.h++"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>

namespace docking_control {

    /* ---- Tuning ------------------------------------------------------------------------------ */

    /** How far from the station a docking request can be made, in world units. */
    constexpr float requestRange = 20000.f;

    /** Seconds the station takes to answer a request. */
    constexpr float requestDelay = 2.f;

    /** A queued request waits this long for the slot to clear, then is refused. */
    constexpr float queueTimeout = 45.f;

    /** Once the slot clears, the station waits this long before granting a queued request. */
    constexpr float clearanceDelay = 1.5f;

    /** How long a granted permit lasts, in seconds. */
    constexpr float permitDuration = 300.f;

    /** Within this distance of the slot mouth, a permit puts the ship in docking mode. */
    constexpr float dockingModeRange = 3000.f;

    /** Within this distance, a permit holder reserves the slot: NPC ships wait for it. */
    constexpr float reservationRange = 4000.f;

    /**
     * The docking speed limit, in units per second relative to the station: 8% of the distance to
     * the slot, kept between a floor and a ceiling, and a crawl once inside it.
     */
    constexpr float speedLimitGain = 0.08f;
    constexpr float speedLimitFloor = 30.f;
    constexpr float speedLimitCeiling = 120.f;
    constexpr float speedLimitInside = 20.f;

    /** Docking completes once the ship's centre is this far short of the docked position (nose near the back wall)... */
    constexpr float completionMargin = 40.f;

    /** ...and the ship is moving slower than this relative to the station. */
    constexpr float completionSpeed = 25.f;

    /** A hull contact at or above this closing speed counts as a scrape (and is fined). */
    constexpr float scrapeSpeed = 5.f;

    /** Seconds after a scrape before another can be counted, so one bounce isn't fined several times. */
    constexpr float scrapeCooldownSeconds = 1.5f;

    /** The third scrape under one permit revokes it. */
    constexpr int maxScrapes = 3;

    /** How long the docking key card stays on screen after docking mode begins. */
    constexpr float keyCardSeconds = 15.f;

    /**
     * Whether docking mode matches the ship's roll rate to the slot's spin, so an untouched ship
     * stays aligned and the pilot only corrects the angle. Switch it off to make the pilot hold the
     * roll rate by hand (as in Elite 1984): the slot turns once every 42 seconds at the default spin.
     */
    constexpr bool rollAssist = true;

    /* ---- The permit -------------------------------------------------------------------------- */

    /** Where a docking request has got to. */
    enum class PermitState { None, Requesting, Queued, Granted };

    /** The pilot's dealings with the station's docking control. */
    struct DockingControl {
        PermitState state = PermitState::None;

        /** Seconds into Requesting or Queued, and how long the slot has been clear while Queued. */
        float timer = 0.f;
        float clearTimer = 0.f;

        /** Seconds left on a granted permit. */
        float permitRemaining = 0.f;

        /** The pilot pressed the auto-dock key without a permit: engage the docking computer once it's granted. */
        bool autoDockWhenGranted = false;

        /** Hull contacts counted under the current permit, and the cooldown before the next one counts. */
        int scrapes = 0;
        float scrapeCooldown = 0.f;

        /** Seconds left to show the docking key card. */
        float keyCardTimer = 0.f;

        /** Whether the ship was in docking mode last step (to spot the moment it begins and ends). */
        bool wasDockingMode = false;

        /** What the station just said, for the scene to show; the scene clears it. */
        std::string event;
    };

    /** Distance from the ship to the slot mouth, in world units. */
    inline float distanceToMouth(const World& world)
    {
        return length(world.playerShip.position - stationDockMouth(world.station));
    }

    /** True while an NPC ship is in the slot (entering it or launching out of it). */
    inline bool slotOccupiedByNpc(const World& world)
    {
        return std::any_of(world.npcShips.begin(), world.npcShips.end(), [](const NpcShip& npc)
        {
            return npc.state == NpcState::EnteringStation || npc.state == NpcState::Launching;
        });
    }

    /** A distance as "12.4K" or "950", for messages. */
    inline std::string shortDistance(float distance)
    {
        char buffer[32];

        if (distance >= 1000.f)
            std::snprintf(buffer, sizeof(buffer), "%.1fK", distance / 1000.f);
        else
            std::snprintf(buffer, sizeof(buffer), "%.0f", distance);

        return buffer;
    }

    /**
     * Asks the station for a docking slot. Refused at once if there is no station or the ship is out
     * of range; otherwise the request starts, and the station answers after requestDelay.
     */
    inline bool requestDocking(DockingControl& control, const World& world)
    {
        if (!world.stationActive || !world.station.dockingPort.valid)
        {
            control.event = "NO STATION IN THIS SYSTEM";
            return false;
        }

        const float distance = length(world.playerShip.position - world.station.position);

        if (distance > requestRange)
        {
            control.event = "STATION OUT OF RANGE: " + shortDistance(distance) + " (LIMIT " + shortDistance(requestRange) + ")";
            return false;
        }

        control.state = PermitState::Requesting;
        control.timer = 0.f;
        control.clearTimer = 0.f;
        control.scrapes = 0;
        control.event = "DOCKING REQUEST SENT";
        return true;
    }

    /** Gives up a request or permit. */
    inline void cancelPermit(DockingControl& control, const std::string& reason)
    {
        const bool hadSomething = control.state != PermitState::None;
        control.state = PermitState::None;
        control.timer = 0.f;
        control.permitRemaining = 0.f;
        control.autoDockWhenGranted = false;
        control.scrapes = 0;

        if (hadSomething)
            control.event = reason;
    }

    /** Steps the request and permit along: the station's reply, the queue, and the permit running out. */
    inline void updateControl(DockingControl& control, const World& world, float dt)
    {
        control.scrapeCooldown = std::max(0.f, control.scrapeCooldown - dt);
        control.keyCardTimer = std::max(0.f, control.keyCardTimer - dt);

        const auto grant = [&]()
        {
            control.state = PermitState::Granted;
            control.permitRemaining = permitDuration;
            control.scrapes = 0;
            control.event = "DOCKING GRANTED: PROCEED TO THE SLOT";
        };

        switch (control.state)
        {
            case PermitState::None:
                break;

            case PermitState::Requesting:
                control.timer += dt;

                if (control.timer >= requestDelay)
                {
                    if (slotOccupiedByNpc(world))
                    {
                        control.state = PermitState::Queued;
                        control.timer = 0.f;
                        control.clearTimer = 0.f;
                        control.event = "REQUEST QUEUED: SLOT BUSY";
                    }
                    else
                    {
                        grant();
                    }
                }
                break;

            case PermitState::Queued:
                control.timer += dt;

                if (slotOccupiedByNpc(world))
                    control.clearTimer = 0.f;
                else
                    control.clearTimer += dt;

                if (control.clearTimer >= clearanceDelay)
                    grant();
                else if (control.timer >= queueTimeout)
                    cancelPermit(control, "DENIED: NO SLOT AVAILABLE");
                break;

            case PermitState::Granted:
                control.permitRemaining -= dt;

                if (control.permitRemaining <= 0.f)
                    cancelPermit(control, "DOCKING PERMIT EXPIRED");
                break;
        }
    }

    /** The speed limit at `progress` (the ship's distance from the mouth plane along the slot normal; negative = inside). */
    inline float speedLimitAt(float progress)
    {
        if (progress < 0.f)
            return speedLimitInside;

        return std::clamp(progress * speedLimitGain, speedLimitFloor, speedLimitCeiling);
    }

    /** True while a permit is held, the docking computer isn't flying, and the ship is close enough to the slot. */
    inline bool inDockingMode(const DockingControl& control, const World& world, DockingPhase computerPhase)
    {
        return control.state == PermitState::Granted &&
               computerPhase == DockingPhase::Idle &&
               world.stationActive &&
               distanceToMouth(world) < dockingModeRange;
    }

    /**
     * True while the player holds the slot: the docking computer is flying the ship in or out, or a
     * permit is held and the ship is near. NPC ships wait while this is true. (A ship sitting docked
     * doesn't hold it, or the station's traffic would stop for as long as the player stayed.)
     */
    inline bool slotReservedByPlayer(const DockingControl& control, const World& world, DockingPhase computerPhase)
    {
        if (computerPhase != DockingPhase::Idle && computerPhase != DockingPhase::Docked)
            return true;

        return control.state == PermitState::Granted && world.stationActive && distanceToMouth(world) < reservationRange;
    }

    /* ---- Guidance ---------------------------------------------------------------------------- */

    /**
     * Everything the pilot needs to line up with the slot, in the slot's own frame: where the ship
     * is, how fast it is closing, and how far off its nose and wings are. Positive offsets are along
     * the slot's long side (u) and across it (v, from cross(normal, slotAxis)).
     */
    struct DockingGuidance {
        bool active = false;
        bool computerFlying = false;

        /** The slot in world space: mouth centre, outward normal, long axis, cross axis, and half-sizes of the opening. */
        Vec3 mouth;
        Vec3 normal;
        Vec3 slotAxis;
        Vec3 vertical;
        float halfWidth = 0.f;
        float halfHeight = 0.f;

        /**
         * The axes the offsets and errors below are measured along, turned to match the pilot's view.
         * The slot is symmetric, so a ship can be matched to it two ways round (180 degrees apart);
         * these are slotAxis and vertical, flipped if the ship is rolled the other way, so that
         * positive u is always the pilot's right and positive v the pilot's down.
         */
        Vec3 axisU;
        Vec3 axisV;

        /** The ship's half-extents (wingspan and thickness), so the panel can draw its footprint. */
        float shipHalfSpan = 250.f;
        float shipHalfThickness = 51.f;

        /** Ship centre's distance from the mouth plane (negative inside) and how far it still has to go to dock. */
        float progress = 0.f;
        float range = 0.f;

        /** Ship centre's offset from the slot axis. */
        float offsetU = 0.f;
        float offsetV = 0.f;

        /** Speed relative to the station: along the slot (positive = inward) and sideways. */
        float closingSpeed = 0.f;
        float lateralU = 0.f;
        float lateralV = 0.f;

        /** How far the nose is from pointing straight into the slot, along u and v, in radians. */
        float alignU = 0.f;
        float alignV = 0.f;

        /** How far the wings are from matching the slot, in radians (positive: the ship needs to roll left). */
        float rollError = 0.f;

        /** The current speed limit and the seconds left on the permit. */
        float speedLimit = 0.f;
        float permitRemaining = 0.f;

        /** Whether to show the docking key card (for the first seconds of docking mode). */
        bool showKeyCard = false;
    };

    /** The guidance numbers for the ship's present position relative to the slot. */
    inline DockingGuidance computeGuidance(const World& world, const DockingControl& control, DockingPhase computerPhase)
    {
        DockingGuidance guidance;

        if (!world.stationActive || !world.station.dockingPort.valid)
            return guidance;

        const Ship& ship = world.playerShip;
        const Station& station = world.station;
        guidance.mouth = stationDockMouth(station);
        guidance.normal = stationDockNormal(station);
        guidance.slotAxis = stationDockSlotAxis(station);
        guidance.vertical = stationDockVertical(station);
        guidance.halfWidth = station.dockingPort.halfWidth;
        guidance.halfHeight = station.dockingPort.halfHeight;

        float span = 0.f;
        float thickness = 0.f;

        for (const Vec3& vertex : ship.model.vertices)
        {
            span = std::max(span, std::abs(vertex.x));
            thickness = std::max(thickness, std::abs(vertex.y));
        }

        if (span > 0.f)
        {
            guidance.shipHalfSpan = span;
            guidance.shipHalfThickness = thickness;
        }

        const float orientation = dot(shipRight(ship), guidance.slotAxis) >= 0.f ? 1.f : -1.f;
        guidance.axisU = guidance.slotAxis * orientation;
        guidance.axisV = guidance.vertical * orientation;

        const Vec3 offset = ship.position - guidance.mouth;
        const Vec3 relativeVelocity = ship.velocity - stationVelocity(world);
        const Vec3 inward = guidance.normal * -1.f;
        const Vec3 forward = shipForward(ship);
        const float dockedProgress = -std::max(0.f, station.dockingPort.depth - docking::shipHalfLength(ship) - docking::noseClearance);

        guidance.active = true;
        guidance.computerFlying = computerPhase != DockingPhase::Idle;
        guidance.progress = dot(offset, guidance.normal);
        guidance.range = guidance.progress - dockedProgress;
        guidance.offsetU = dot(offset, guidance.axisU);
        guidance.offsetV = dot(offset, guidance.axisV);
        guidance.closingSpeed = -dot(relativeVelocity, guidance.normal);
        guidance.lateralU = dot(relativeVelocity, guidance.axisU);
        guidance.lateralV = dot(relativeVelocity, guidance.axisV);
        guidance.alignU = std::asin(std::clamp(dot(forward, guidance.axisU), -1.f, 1.f));
        guidance.alignV = std::asin(std::clamp(dot(forward, guidance.axisV), -1.f, 1.f));
        guidance.rollError = wrapAngle(docking::rollToMatchSlot(ship, inward, guidance.slotAxis) - ship.roll);
        guidance.speedLimit = speedLimitAt(guidance.progress);
        guidance.permitRemaining = control.permitRemaining;
        guidance.showKeyCard = control.keyCardTimer > 0.f;
        return guidance;
    }

    /* ---- Docking mode, applied to the ship --------------------------------------------------- */

    /**
     * Sets the ship up for the current step: in docking mode, flight assist measures speed against
     * the station and is capped by the speed limit, the roll rate follows the slot's spin so an
     * untouched ship stays aligned, and flight assist is forced on. Out of docking mode everything
     * returns to normal. Entering or leaving docking mode re-points the throttle at the speed the
     * ship already has, so the change of reference doesn't brake or lurch it.
     */
    inline void applyDockingMode(Ship& ship, const World& world, bool dockingMode)
    {
        const bool changed = ship.dockingMode != dockingMode;
        ship.dockingMode = dockingMode;

        if (dockingMode)
        {
            const Station& station = world.station;
            const Vec3 normal = stationDockNormal(station);

            ship.flightAssist = true;
            ship.assistFrameVelocity = stationVelocity(world);
            ship.speedLimit = speedLimitAt(dot(ship.position - stationDockMouth(station), normal));

            // The ship rolls about its nose; the slot spins about its normal. Their match is the
            // station's spin component along the ship's forward axis.
            ship.rollFeedForward = rollAssist ? dot(stationAngularVelocity(station), shipForward(ship)) : 0.f;
        }
        else
        {
            ship.assistFrameVelocity = {};
            ship.speedLimit = 0.f;
            ship.rollFeedForward = 0.f;
            ship.strafeRightInput = 0.f;
            ship.strafeUpInput = 0.f;
        }

        if (changed)
            matchThrottleToVelocity(ship);
    }

    /** True when a manual docking is complete: deep enough in the slot, and slow enough relative to the station. */
    inline bool manualDockingComplete(const World& world)
    {
        if (!world.stationActive || !world.station.dockingPort.valid)
            return false;

        const Ship& ship = world.playerShip;
        const Station& station = world.station;
        const float dockedProgress = -std::max(0.f, station.dockingPort.depth - docking::shipHalfLength(ship) - docking::noseClearance);
        const float progress = dot(ship.position - stationDockMouth(station), stationDockNormal(station));
        const float speed = length(ship.velocity - stationVelocity(world));

        return progress <= dockedProgress + completionMargin && speed <= completionSpeed;
    }

} // namespace docking_control

#endif //DUSK_DOCKING_CONTROL_H
