//
// Created by Mykyta Khomiakov on 16/09/2026.
//

#ifndef DUSK_DOCKING_COMPUTER_H
#define DUSK_DOCKING_COMPUTER_H

#include "math/Vec3.h++"
#include "objects/cube.h++"
#include "objects/planet.h++"
#include "objects/ship.h++"
#include "world/world.h++"
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

/**
 * Stages of the automatic docking sequence.
 *
 *   Idle -> Approach -> Align -> Enter -> Docked -> LaunchReverse -> LaunchTurn -> Idle
 *
 * Approach/Align can be cancelled, which passes through Disengage (roll back to level) before Idle.
 */
enum class DockingPhase {
    Idle,
    Approach,       // fly to the approach point in front of the slot, avoiding planets and the star
    Align,          // hold at the approach point, point the nose into the slot and match the station's spin
    Enter,          // slide along the slot axis into the station
    Docked,         // parked inside the slot; the station menu is available
    LaunchReverse,  // back straight out of the slot
    LaunchTurn,     // turn to face away from the station and level the wings
    Disengage       // cancelled mid-approach: level the wings, then hand control back
};

/** Runtime state of the player's docking computer. */
struct DockingComputer {
    DockingPhase phase = DockingPhase::Idle;

    /** Distance of the ship's centre from the slot mouth along the slot normal (negative = inside). */
    float slotProgress = 0.f;

    /** Seconds spent in the current phase. */
    float phaseTimer = 0.f;

    /** Slot mouth and approach point last frame, used to estimate how fast they are moving. */
    Vec3 previousMouth;
    Vec3 previousApproachPoint;
    bool hasPreviousFrame = false;

    /** Ship velocity relative to the moving approach point, kept as state so the target's own
     *  acceleration doesn't leak into the controller as a steady-state error. */
    Vec3 relativeVelocity;
    bool relativeVelocityValid = false;

    /** Short status line for the HUD, with a timer for transient messages. */
    std::string message;
    float messageTimer = 0.f;
};

namespace docking {

constexpr float pi = 3.14159265358979323846f;

/** Distance in front of the slot where the ship lines up before entering. */
constexpr float approachDistance = 1600.f;

/** Top speed relative to the station while approaching, in world units per second. */
constexpr float maxApproachSpeed = 3000.f;

/** Approach speed per unit of remaining distance; gives a smooth slowdown on arrival. */
constexpr float approachSpeedGain = 0.9f;

/** How quickly the ship's velocity converges on the commanded velocity (per second). */
constexpr float velocityResponse = 3.f;

/** Position error at which the approach point counts as reached, and the error allowed before entry. */
constexpr float approachArrivalRadius = 60.f;
constexpr float entryPositionTolerance = 5.f;

/** Turn rates for yaw/pitch and for roll, in radians per second. */
constexpr float turnRate = 1.2f;
constexpr float rollRate = 1.4f;

/** Heading and roll error allowed before starting the slot entry. */
constexpr float alignTolerance = 0.02f;

/** Speed along the slot axis while entering and reversing out. */
constexpr float maxEntrySpeed = 450.f;
constexpr float minEntrySpeed = 90.f;

/** Where the ship stops after reversing out, as distance from the mouth. */
constexpr float launchClearance = 900.f;

/** Speed away from the station when control is handed back after launch. */
constexpr float launchReleaseSpeed = 300.f;

/** Gap left between the ship's nose and the back wall of the slot. */
constexpr float noseClearance = 10.f;

/** Extra clearance around planets and the star when planning the approach path. */
constexpr float bodyAvoidanceScale = 1.25f;
constexpr float bodyAvoidanceMargin = 400.f;
constexpr float stationAvoidanceMargin = 300.f;

/** Within this distance of a planet/star surface the approach heading is bent away from it. */
constexpr float avoidanceZone = 1500.f;

/** Within this distance of a surface, any velocity into the body is removed outright. */
constexpr float hardAvoidanceZone = 250.f;

/** Near planets and the star the approach slows down so the ship can turn around them in time. */
constexpr float clearanceSpeedBase = 300.f;
constexpr float clearanceSpeedGain = 0.6f;

/** The station's own motion is matched fully inside the near range and ignored beyond the far range. */
constexpr float matchVelocityNearRange = 3000.f;
constexpr float matchVelocityFarRange = 9000.f;

/** Returns true while the computer, not the player, is flying the ship. */
inline bool controlsShip(const DockingComputer& computer)
{
    return computer.phase != DockingPhase::Idle;
}

/** Returns true while cancelling is still allowed. */
inline bool canCancel(const DockingComputer& computer)
{
    return computer.phase == DockingPhase::Approach || computer.phase == DockingPhase::Align;
}

inline void setPhase(DockingComputer& computer, DockingPhase phase)
{
    computer.phase = phase;
    computer.phaseTimer = 0.f;
}

inline void showMessage(DockingComputer& computer, std::string text, float seconds = 2.5f)
{
    computer.message = std::move(text);
    computer.messageTimer = seconds;
}

/** Half the ship's length along its local forward axis, measured from the loaded model. */
inline float shipHalfLength(const Ship& ship)
{
    float front = 0.f;

    for (const Vec3& vertex : ship.model.vertices)
        front = std::max(front, vertex.z);

    return front;
}

/** Wraps an angle into (-pi, pi]. */
inline float wrap(float angle)
{
    return wrapAngle(angle);
}

/** Moves `current` toward `target` by at most maxStep, taking the short way round. */
inline float approachAngle(float current, float target, float maxStep)
{
    return wrap(current + std::clamp(wrap(target - current), -maxStep, maxStep));
}

/**
 * Turns the ship toward a world direction and a roll angle at limited rates.
 * Returns the largest remaining error in radians.
 */
inline float steerShip(Ship& ship, const Vec3& direction, float targetRoll, float dt)
{
    const float desiredYaw = std::atan2(direction.x, direction.z);
    const float desiredPitch = std::asin(std::clamp(direction.y, -1.f, 1.f));

    ship.yaw = approachAngle(ship.yaw, desiredYaw, turnRate * dt);
    ship.pitch = std::clamp(
        ship.pitch + std::clamp(desiredPitch - ship.pitch, -turnRate * dt, turnRate * dt),
        -1.25f,
        1.25f
    );
    ship.roll = approachAngle(ship.roll, targetRoll, rollRate * dt);

    return std::max({
        std::abs(wrap(desiredYaw - ship.yaw)),
        std::abs(desiredPitch - ship.pitch),
        std::abs(wrap(targetRoll - ship.roll))
    });
}

/**
 * Roll that lines the ship's wings up with the slot's long side when flying along `forward`.
 * The slot is symmetric, so of the two matching angles the one nearest the current roll wins.
 */
inline float rollToMatchSlot(const Ship& ship, const Vec3& forward, const Vec3& slotAxis)
{
    Ship probe = ship;
    probe.yaw = std::atan2(forward.x, forward.z);
    probe.pitch = std::asin(std::clamp(forward.y, -1.f, 1.f));
    probe.roll = 0.f;

    const Vec3 right = shipUnrolledRight(probe);
    const Vec3 up = shipUnrolledUp(probe);
    const float roll = std::atan2(dot(slotAxis, up), dot(slotAxis, right));
    const float flipped = wrap(roll + pi);

    return std::abs(wrap(roll - ship.roll)) <= std::abs(wrap(flipped - ship.roll)) ? roll : flipped;
}

/** Largest distance of any model vertex from the station centre. */
inline float stationBoundingRadius(const Station& station)
{
    float radius = 0.f;

    for (const Vec3& vertex : station.model.vertices)
        radius = std::max(radius, length(vertex));

    return radius;
}

/**
 * Returns the point to fly toward: the target itself, or a detour point beside the nearest
 * obstacle (star, planet, or the station hull) that sits on the straight path.
 */
inline Vec3 pathWaypoint(const Vec3& position, const Vec3& target, const World& world)
{
    const Vec3 path = target - position;
    const float pathLength = length(path);

    if (pathLength <= 0.f)
        return target;

    const Vec3 direction = path / pathLength;
    bool blocked = false;
    float blockerAlong = pathLength;
    Vec3 blockerCentre;
    Vec3 blockerClosest;
    float blockerRadius = 0.f;

    const auto check = [&](const Vec3& centre, float surfaceRadius, float safeRadius)
    {
        // If the target sits inside the usual safety zone, tighten the zone to just short of the
        // target instead of dropping the obstacle, so the path still can't cut through its surface.
        const float targetDistance = length(target - centre);

        if (targetDistance <= surfaceRadius)
            return;

        safeRadius = std::min(safeRadius, std::max(surfaceRadius + 1.f, targetDistance - 1.f));

        const float along = std::clamp(dot(centre - position, direction), 0.f, pathLength);
        const Vec3 closest = position + direction * along;

        if (length(closest - centre) >= safeRadius || along >= blockerAlong)
            return;

        blocked = true;
        blockerAlong = along;
        blockerCentre = centre;
        blockerClosest = closest;
        blockerRadius = safeRadius;
    };

    const auto bodyRadius = [](const Planet& body)
    {
        return body.radius * bodyAvoidanceScale + bodyAvoidanceMargin;
    };

    check(world.star.position, world.star.radius, bodyRadius(world.star));

    for (const Planet& planet : world.planets)
        check(planet.position, planet.radius, bodyRadius(planet));

    if (world.stationActive)
    {
        const float hull = stationBoundingRadius(world.station);
        check(world.station.position, hull, hull + stationAvoidanceMargin);
    }

    if (!blocked)
        return target;

    Vec3 sideways = blockerClosest - blockerCentre;

    // Path goes straight through the centre: pick any direction perpendicular to it.
    if (length(sideways) < 1.f)
    {
        sideways = cross(direction, Vec3{0.f, 1.f, 0.f});

        if (length(sideways) < 1e-3f)
            sideways = cross(direction, Vec3{1.f, 0.f, 0.f});
    }

    return blockerCentre + normalized(sideways) * blockerRadius * 1.2f;
}

/** Distance from a point to the nearest planet or star surface. */
inline float surfaceClearance(const Vec3& position, const World& world)
{
    float clearance = length(position - world.star.position) - world.star.radius;

    for (const Planet& planet : world.planets)
        clearance = std::min(clearance, length(position - planet.position) - planet.radius);

    return clearance;
}

/** Calls fn(centre, radius) for the star and every planet. */
template <typename Fn>
inline void forEachBody(const World& world, Fn&& fn)
{
    fn(world.star.position, world.star.radius);

    for (const Planet& planet : world.planets)
        fn(planet.position, planet.radius);
}

/**
 * Bends a heading away from nearby planet and star surfaces: the part pointing into a body is
 * removed and a gentle outward push is added. `zone` is how far out from a surface this acts.
 */
inline Vec3 avoidBodies(const Vec3& position, Vec3 heading, const World& world, float zone)
{
    if (zone <= 0.f || length(heading) == 0.f)
        return heading;

    forEachBody(world, [&](const Vec3& centre, float radius)
    {
        const Vec3 offset = position - centre;
        const float distance = length(offset);
        const float clearance = distance - radius;

        if (distance <= 0.f || clearance >= zone)
            return;

        const Vec3 outward = offset / distance;
        const float inward = -dot(heading, outward);

        if (inward > 0.f)
            heading += outward * inward;

        heading += outward * (1.f - std::max(0.f, clearance) / zone) * 0.6f;
    });

    return length(heading) > 0.f ? normalized(heading) : heading;
}

/** Removes any velocity component that would carry the ship into a surface it is already skimming. */
inline Vec3 constrainVelocityAgainstBodies(const Vec3& position, Vec3 velocity, const World& world, float zone)
{
    forEachBody(world, [&](const Vec3& centre, float radius)
    {
        const Vec3 offset = position - centre;
        const float distance = length(offset);

        if (distance <= 0.f || distance - radius >= zone)
            return;

        const Vec3 outward = offset / distance;
        const float inwardSpeed = -dot(velocity, outward);

        if (inwardSpeed > 0.f)
            velocity += outward * inwardSpeed;
    });

    return velocity;
}

/** Moves a point that has slipped below a planet/star surface back onto it. */
inline Vec3 pushOutOfBodies(Vec3 position, const World& world)
{
    forEachBody(world, [&](const Vec3& centre, float radius)
    {
        const Vec3 offset = position - centre;
        const float distance = length(offset);

        if (distance > 0.f && distance < radius)
            position = centre + offset / distance * radius;
    });

    return position;
}

/** Starts the docking sequence if there is a station with a usable slot. */
inline bool engage(DockingComputer& computer, const World& world)
{
    if (computer.phase != DockingPhase::Idle)
        return false;

    if (!world.stationActive)
    {
        showMessage(computer, "NO STATION IN THIS SYSTEM");
        return false;
    }

    if (!world.station.dockingPort.valid)
    {
        showMessage(computer, "STATION HAS NO DOCKING SLOT");
        return false;
    }

    computer.hasPreviousFrame = false;
    computer.relativeVelocityValid = false;
    setPhase(computer, DockingPhase::Approach);
    showMessage(computer, "DOCKING COMPUTER ENGAGED");
    return true;
}

/** Aborts an approach; the ship levels its wings and then control returns to the player. */
inline bool cancel(DockingComputer& computer)
{
    if (!canCancel(computer))
    {
        if (computer.phase == DockingPhase::Enter)
            showMessage(computer, "DOCKING COMMITTED");

        return false;
    }

    setPhase(computer, DockingPhase::Disengage);
    showMessage(computer, "DOCKING CANCELLED");
    return true;
}

/** Leaves the station: reverses out of the slot, turns around, and hands back control. */
inline bool launch(DockingComputer& computer)
{
    if (computer.phase != DockingPhase::Docked)
        return false;

    setPhase(computer, DockingPhase::LaunchReverse);
    showMessage(computer, "LAUNCHING");
    return true;
}

/** Returns control to the player with the ship coasting at the given velocity. */
inline void releaseControl(DockingComputer& computer, Ship& ship, const Vec3& velocity)
{
    ship.velocity = velocity;
    ship.throttle = 0.f;
    ship.reverseThrust = false;
    ship.roll = 0.f;
    clampShipPitch(ship);
    setPhase(computer, DockingPhase::Idle);
}

/** Places the ship on the slot axis at the current progress, keeping it moving with the station. */
inline void holdOnSlotAxis(
    Ship& ship,
    const Vec3& mouth,
    const Vec3& normal,
    float progress,
    const Vec3& previousPosition,
    float dt
)
{
    ship.position = mouth + normal * progress;
    ship.velocity = dt > 0.f ? (ship.position - previousPosition) / dt : Vec3{};
}

/**
 * Advances the docking sequence by one frame. Call after the world (and so the station) has
 * been updated, with the player ship excluded from normal physics integration.
 */
inline void update(DockingComputer& computer, World& world, float dt)
{
    computer.messageTimer = std::max(0.f, computer.messageTimer - dt);

    if (computer.phase == DockingPhase::Idle)
        return;

    Ship& ship = world.playerShip;
    ship.previousPosition = ship.position;
    ship.throttle = 0.f;
    ship.reverseThrust = false;
    computer.phaseTimer += dt;

    if (computer.phase == DockingPhase::Disengage)
    {
        const float remaining = steerShip(ship, shipForward(ship), 0.f, dt);
        ship.position = ship.position + ship.velocity * dt;

        if (remaining < 1e-3f)
            releaseControl(computer, ship, ship.velocity);

        return;
    }

    // The station vanished (e.g. system change mid-sequence): hand control straight back.
    if (!world.stationActive || !world.station.dockingPort.valid)
    {
        releaseControl(computer, ship, ship.velocity);
        return;
    }

    const Station& station = world.station;
    const Vec3 mouth = stationDockMouth(station);
    const Vec3 normal = stationDockNormal(station);
    const Vec3 slotAxis = stationDockSlotAxis(station);
    const Vec3 inward = normal * -1.f;

    const Vec3 approachPoint = mouth + normal * approachDistance;

    // Both points sweep around the host planet at different radii, so each needs its own velocity.
    const bool hasHistory = computer.hasPreviousFrame && dt > 0.f;
    const Vec3 slotVelocity = hasHistory ? (mouth - computer.previousMouth) / dt : Vec3{};
    const Vec3 approachVelocity = hasHistory ? (approachPoint - computer.previousApproachPoint) / dt : Vec3{};
    computer.previousMouth = mouth;
    computer.previousApproachPoint = approachPoint;
    computer.hasPreviousFrame = true;

    const float halfLength = shipHalfLength(ship);
    const float dockedProgress = -std::max(0.f, station.dockingPort.depth - halfLength - noseClearance);

    switch (computer.phase)
    {
        case DockingPhase::Approach:
        case DockingPhase::Align:
        {
            // Far out, fly straight at the target; close in, ride along with the moving approach point.
            const float rawDistance = length(approachPoint - ship.position);
            const float match = std::clamp(
                (matchVelocityFarRange - rawDistance) / (matchVelocityFarRange - matchVelocityNearRange),
                0.f,
                1.f
            );
            const Vec3 carrierVelocity = hasHistory ? approachVelocity * match : Vec3{};

            // Where the ship would be this frame if it simply rode along with that carrier velocity.
            const Vec3 carried = ship.position + (hasHistory ? carrierVelocity * dt : ship.velocity * dt);
            const float distance = length(approachPoint - carried);
            const Vec3 waypoint = computer.phase == DockingPhase::Approach
                ? pathWaypoint(carried, approachPoint, world)
                : approachPoint;
            const Vec3 toWaypoint = waypoint - carried;
            Vec3 heading = length(toWaypoint) > 0.f ? normalized(toWaypoint) : Vec3{};

            // Surface avoidance fades out near the approach point so it never fights the final line-up.
            const float targetClearance = std::max(0.f, surfaceClearance(approachPoint, world));
            const float zone = std::min(avoidanceZone, targetClearance * 0.8f);
            const float hardZone = std::clamp(targetClearance * 0.5f, 25.f, hardAvoidanceZone);

            if (computer.phase == DockingPhase::Approach)
                heading = avoidBodies(carried, heading, world, zone);

            const float clearanceLimit =
                clearanceSpeedBase + std::max(0.f, surfaceClearance(carried, world)) * clearanceSpeedGain;
            const float speed = std::min({maxApproachSpeed, distance * approachSpeedGain, clearanceLimit});

            if (hasHistory)
            {
                if (!computer.relativeVelocityValid)
                {
                    computer.relativeVelocity = ship.velocity - carrierVelocity;
                    computer.relativeVelocityValid = true;
                }

                // Smooth only the velocity relative to the carrier, so the station's orbital
                // motion is followed exactly instead of lagging behind.
                Vec3& relative = computer.relativeVelocity;
                relative += (heading * speed - relative) * std::min(1.f, velocityResponse * dt);

                Vec3 velocity = carrierVelocity + relative;

                if (computer.phase == DockingPhase::Approach)
                    velocity = constrainVelocityAgainstBodies(ship.position, velocity, world, hardZone);

                relative = velocity - carrierVelocity;
                ship.velocity = velocity;
                ship.position += velocity * dt;

                // Last line of defence while skimming a surface. Skipped if the target itself is
                // inside a body (a station orbiting through its star), where there is no clean path.
                if (computer.phase == DockingPhase::Approach && surfaceClearance(approachPoint, world) > 0.f)
                    ship.position = pushOutOfBodies(ship.position, world);
            }
            else
            {
                ship.position = carried;
            }

            // Face the direction of travel while far out; swing toward the slot on arrival.
            const bool closing = distance < approachDistance;
            const Vec3 lookDirection = closing || length(heading) == 0.f ? inward : heading;
            const float roll = closing ? rollToMatchSlot(ship, inward, slotAxis) : 0.f;
            const float error = steerShip(ship, lookDirection, roll, dt);

            const float remaining = length(approachPoint - ship.position);

            if (computer.phase == DockingPhase::Approach && remaining < approachArrivalRadius)
            {
                setPhase(computer, DockingPhase::Align);
                showMessage(computer, "ALIGNING WITH SLOT");
            }
            else if (computer.phase == DockingPhase::Align &&
                     error < alignTolerance &&
                     remaining < entryPositionTolerance)
            {
                computer.slotProgress = approachDistance;
                setPhase(computer, DockingPhase::Enter);
                showMessage(computer, "ENTERING STATION");
            }
            break;
        }

        case DockingPhase::Enter:
        {
            const float remaining = computer.slotProgress - dockedProgress;
            const float speed = std::clamp(remaining * 0.5f, minEntrySpeed, maxEntrySpeed);
            computer.slotProgress = std::max(dockedProgress, computer.slotProgress - speed * dt);

            holdOnSlotAxis(ship, mouth, normal, computer.slotProgress, ship.previousPosition, dt);
            steerShip(ship, inward, rollToMatchSlot(ship, inward, slotAxis), dt);

            if (computer.slotProgress <= dockedProgress)
            {
                setPhase(computer, DockingPhase::Docked);
                showMessage(computer, "DOCKED", 1.5f);
            }
            break;
        }

        case DockingPhase::Docked:
        {
            computer.slotProgress = dockedProgress;
            holdOnSlotAxis(ship, mouth, normal, computer.slotProgress, ship.previousPosition, dt);
            steerShip(ship, inward, rollToMatchSlot(ship, inward, slotAxis), dt);
            break;
        }

        case DockingPhase::LaunchReverse:
        {
            const float remaining = launchClearance - computer.slotProgress;
            const float speed = std::clamp(remaining * 0.5f, minEntrySpeed, maxEntrySpeed);
            computer.slotProgress = std::min(launchClearance, computer.slotProgress + speed * dt);

            holdOnSlotAxis(ship, mouth, normal, computer.slotProgress, ship.previousPosition, dt);
            steerShip(ship, inward, rollToMatchSlot(ship, inward, slotAxis), dt);

            if (computer.slotProgress >= launchClearance)
            {
                setPhase(computer, DockingPhase::LaunchTurn);
                showMessage(computer, "CLEAR OF STATION");
            }
            break;
        }

        case DockingPhase::LaunchTurn:
        {
            holdOnSlotAxis(ship, mouth, normal, computer.slotProgress, ship.previousPosition, dt);
            const float error = steerShip(ship, normal, 0.f, dt);

            if (error < alignTolerance)
            {
                ship.yaw = std::atan2(normal.x, normal.z);
                ship.pitch = std::asin(std::clamp(normal.y, -1.f, 1.f));
                releaseControl(computer, ship, slotVelocity + normal * launchReleaseSpeed);
                showMessage(computer, "YOU HAVE CONTROL");
            }
            break;
        }

        case DockingPhase::Idle:
        case DockingPhase::Disengage:
            break;
    }
}

/** HUD label for the current phase, or an empty string when idle. */
inline const char* phaseLabel(DockingPhase phase)
{
    switch (phase)
    {
        case DockingPhase::Approach: return "DOCKING: APPROACH";
        case DockingPhase::Align: return "DOCKING: ALIGNING";
        case DockingPhase::Enter: return "DOCKING: ENTERING";
        case DockingPhase::Docked: return "DOCKED";
        case DockingPhase::LaunchReverse: return "LAUNCHING";
        case DockingPhase::LaunchTurn: return "LAUNCHING";
        case DockingPhase::Disengage: return "DOCKING: DISENGAGING";
        case DockingPhase::Idle: break;
    }

    return "";
}

} // namespace docking

#endif //DUSK_DOCKING_COMPUTER_H
