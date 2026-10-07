//
// Created by Mykyta Khomiakov on 22/07/2026.
//
// Keyboard input for the ship (written as intent: throttle, turn demands, mode toggles) and the
// two ship cameras: the spring-mounted chase camera and the orbiting showcase camera.
//

#ifndef DUSK_SHIP_CONTROLLER_H
#define DUSK_SHIP_CONTROLLER_H

#include "objects/ship.h++"
#include "systems/ship_physics.h++"
#include "tools/camera.h++"
#include <SFML/Window/Keyboard.hpp>
#include <algorithm>
#include <cmath>

/**
 * Chase-camera framing and spring tuning.
 *
 * The camera rides in the ship's own frame (it pitches with the ship, so the ship holds the same
 * place on screen at any attitude) and sits on a damped spring. The spring is driven by the
 * ship's acceleration, exactly as a loosely mounted camera would be: throttle up and it falls
 * back, brake and it surges in, carve a turn and it swings out, then it bounces back to rest.
 */
struct ShipCameraSettings {
    /** Rest distance behind the ship along its forward axis. */
    float followDistance = 470.f;

    /** Rest height above the ship, along the ship's (unrolled) up axis. */
    float followHeight = 190.f;

    /**
     * How far the camera looks down relative to the ship's nose, in degrees. Together with the
     * distance and height this puts the ship just below the middle of the view above the HUD
     * dashboard, rather than half-hidden behind it.
     */
    float lookDownDegrees = 15.f;

    /** Spring natural frequency in radians per second: higher is stiffer and quicker to settle. */
    float springFrequency = 3.2f;

    /** Damping ratio: 1 settles without overshoot; below 1 bounces. 0.45 gives one or two soft bounces. */
    float springDamping = 0.7f;

    /**
     * Offset acceleration per unit of ship acceleration. At the spring's stiffness this moves the
     * camera about 100 units back at full main-engine thrust (70 u/s^2).
     */
    float accelerationGain = 14.f;

    /**
     * Furthest the camera may stretch back from, or compress toward, its rest distance (measured
     * along the forward axis; the whole boom scales, so the viewing angle never changes).
     */
    float maxStretch = 420.f;
    float maxCompress = 120.f;

    /** Furthest the camera may sway sideways or vertically from rest. */
    float maxSway = 150.f;

    /** Ship acceleration is capped at this before driving the spring, so jumps and cruise drops give a lurch, not a teleport. */
    float maxDrivingAcceleration = 20000.f;

    /** Field of view in normal flight, in degrees. */
    float baseFov = 90.f;

    /** Extra field of view at full cruise speed: the view stretches as the ship gets going. */
    float cruiseFovBoost = 14.f;

    /** How far the view narrows (degrees) at the end of a cruise charge, just before it punches out. */
    float chargeFovPull = 5.f;

    /** How quickly the field of view follows its target, per second. */
    float fovResponse = 3.f;

    /** Distance used by the showcase orbit camera. */
    float showcaseDistance = 1400.f;

    /** Height used by the showcase orbit camera. */
    float showcaseHeight = 650.f;

    /** Orbit speed in radians per second while Up Arrow is held. */
    float showcaseSpeed = 1.6f;
};

/** Runtime state for camera modes that need continuity between frames. */
struct ShipCameraRig {
    /** Current orbit angle for the showcase camera. */
    float showcaseAngle = 0.f;

    /** Spring displacement from the rest offset, in ship-local axes (x right, y up, z forward). */
    Vec3 springOffset;

    /** Rate of change of springOffset. */
    Vec3 springVelocity;

    /** Ship velocity on the previous camera update, for measuring acceleration. */
    Vec3 previousShipVelocity;

    /** False until the first update, so the camera doesn't start with a lurch. */
    bool primed = false;

    /** Current field-of-view offset from baseFov, eased toward its target each frame. */
    float fovOffset = 0.f;

    /** Extra field of view requested by the scene (e.g. the hyperspace jump), added on top. */
    float sceneFovOffset = 0.f;
};

/** Tracks edge-triggered ship input such as reverse-thrust, flight-assist and cruise toggling. */
struct ShipInputState {
    /** True while the reverse-toggle key was down on the previous frame. */
    bool reverseToggleWasDown = false;

    /** True while the flight-assist toggle key was down on the previous frame. */
    bool assistToggleWasDown = false;

    /** True while the cruise toggle key was down on the previous frame. */
    bool cruiseToggleWasDown = false;
};

/** Returns true on the frame a key goes down, given last frame's state, and updates that state. */
inline bool keyPressedThisFrame(sf::Keyboard::Key key, bool& wasDown)
{
    const bool down = sf::Keyboard::isKeyPressed(key);
    const bool pressed = down && !wasDown;
    wasDown = down;
    return pressed;
}

/** Converts a pair of opposing keys into a -1 / 0 / +1 axis value. */
inline float keyAxis(sf::Keyboard::Key negative, sf::Keyboard::Key positive)
{
    float value = 0.f;

    if (sf::Keyboard::isKeyPressed(negative))
        value -= 1.f;

    if (sf::Keyboard::isKeyPressed(positive))
        value += 1.f;

    return value;
}

/**
 * Applies keyboard input to the ship before physics and camera follow run. Input only writes
 * intent (throttle, turn demands, mode toggles); ship physics turns that intent into motion.
 */
inline void updateShipFromKeyboard(Ship& ship, float dt, ShipInputState& inputState)
{
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W))
        ship.throttle += ship.throttleChangeSpeed * dt;

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S))
        ship.throttle -= ship.throttleChangeSpeed * dt;

    // X: all stop. With flight assist on, the thrusters then brake the ship to a halt.
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::X))
        ship.throttle = 0.f;

    const bool shift =
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);

    // Roll is always available: Left rolls left, Right rolls right.
    ship.rollInput = keyAxis(sf::Keyboard::Key::Right, sf::Keyboard::Key::Left);

    if (ship.dockingMode && shift)
    {
        // Docking mode: holding Shift turns A/D/Q/E into thrusters that slide the ship (right/left
        // and up/down) instead of turning it, for lining up with the slot without changing heading.
        ship.yawInput = 0.f;
        ship.pitchInput = 0.f;
        ship.strafeRightInput = keyAxis(sf::Keyboard::Key::A, sf::Keyboard::Key::D);
        ship.strafeUpInput = keyAxis(sf::Keyboard::Key::E, sf::Keyboard::Key::Q);
    }
    else
    {
        // Turn demands; the physics step spins the ship toward these rates and stops it on release.
        ship.yawInput = keyAxis(sf::Keyboard::Key::A, sf::Keyboard::Key::D);
        ship.pitchInput = keyAxis(sf::Keyboard::Key::E, sf::Keyboard::Key::Q);
        ship.strafeRightInput = 0.f;
        ship.strafeUpInput = 0.f;
    }

    // Docking mode always uses the fine turn rates; otherwise Shift asks for them.
    ship.precisionInput = ship.dockingMode || shift;

    if (keyPressedThisFrame(sf::Keyboard::Key::Down, inputState.reverseToggleWasDown) && !ship.cruiseEngaged)
    {
        ship.reverseThrust = !ship.reverseThrust;
        ship.throttle = 0.f;
    }

    // Flight assist can't be switched off in docking mode: the speed limit and strafing depend on it.
    if (keyPressedThisFrame(sf::Keyboard::Key::F, inputState.assistToggleWasDown) && !ship.dockingMode)
    {
        ship.flightAssist = !ship.flightAssist;

        // Switching on: hold the current speed instead of braking. Switching off: coast.
        if (ship.flightAssist)
            matchThrottleToVelocity(ship);
        else
            ship.throttle = 0.f;
    }

    if (keyPressedThisFrame(sf::Keyboard::Key::J, inputState.cruiseToggleWasDown))
    {
        // J drops out of cruise, cancels a charge in progress, or starts charging the drive
        // (refused while mass-locked; the HUD shows why).
        if (ship.cruiseEngaged)
            disengageCruise(ship);
        else if (cruiseCharging(ship))
            cancelCruiseCharge(ship);
        else
            beginCruiseCharge(ship);
    }

    ship.throttle = std::clamp(ship.throttle, 0.f, 1.f);
}

/** Points the camera at a world-space target without rolling the view. */
inline void pointCameraAt(Camera& camera, const Vec3& target)
{
    const Vec3 viewDirection = normalized(target - camera.position);
    constexpr float maxCameraPitch = 1.4f;

    camera.yaw = std::atan2(viewDirection.x, viewDirection.z);
    camera.pitch = std::clamp(
        std::asin(std::clamp(viewDirection.y, -1.f, 1.f)),
        -maxCameraPitch,
        maxCameraPitch
    );
    camera.roll = 0.f;
}

/** Soft limit: passes small values through unchanged and eases large ones toward +/- limit. */
inline float softLimit(float value, float limit)
{
    return limit > 0.f ? limit * std::tanh(value / limit) : 0.f;
}

/**
 * Advances the camera spring by one frame. The ship's acceleration, seen from the ship, acts on
 * the camera as an opposite pseudo-force; the spring pulls it back to rest and the damper bleeds
 * off the bounce. The pseudo-force is soft-limited so that even cruise-drive accelerations settle
 * inside the stretch limits, and integration is sub-stepped so it behaves the same at any frame rate.
 */
inline void updateCameraSpring(const Ship& ship, float dt, ShipCameraRig& rig, const ShipCameraSettings& settings)
{
    if (!rig.primed)
    {
        rig.previousShipVelocity = ship.velocity;
        rig.springOffset = {};
        rig.springVelocity = {};
        rig.primed = true;
        return;
    }

    if (dt <= 0.f)
        return;

    Vec3 acceleration = (ship.velocity - rig.previousShipVelocity) / dt;
    rig.previousShipVelocity = ship.velocity;

    const float accelerationMagnitude = length(acceleration);

    if (accelerationMagnitude > settings.maxDrivingAcceleration)
        acceleration = acceleration * (settings.maxDrivingAcceleration / accelerationMagnitude);

    // Ship-local acceleration, using the unrolled axes so the docking computer's roll doesn't
    // swing the camera around the ship.
    const Vec3 right = shipUnrolledRight(ship);
    const Vec3 up = shipUnrolledUp(ship);
    const Vec3 forward = shipForward(ship);
    const Vec3 local = {dot(acceleration, right), dot(acceleration, up), dot(acceleration, forward)};

    const float stiffness = settings.springFrequency * settings.springFrequency;
    const float damping = 2.f * settings.springDamping * settings.springFrequency;

    // A camera that lags behind an accelerating ship drifts the opposite way: forward thrust
    // pushes it back (negative z), braking pushes it in. Limits are expressed as spring force so
    // the resting displacement can never exceed them.
    const Vec3 push =
    {
        softLimit(-local.x * settings.accelerationGain, stiffness * settings.maxSway),
        softLimit(-local.y * settings.accelerationGain, stiffness * settings.maxSway),
        -local.z * settings.accelerationGain < 0.f
            ? softLimit(-local.z * settings.accelerationGain, stiffness * settings.maxStretch)
            : softLimit(-local.z * settings.accelerationGain, stiffness * settings.maxCompress)
    };

    constexpr float maxStep = 1.f / 120.f;
    const int steps = std::max(1, static_cast<int>(std::ceil(dt / maxStep)));
    const float step = dt / static_cast<float>(steps);

    for (int i = 0; i < steps; ++i)
    {
        const Vec3 springAcceleration = push - rig.springOffset * stiffness - rig.springVelocity * damping;
        rig.springVelocity += springAcceleration * step;
        rig.springOffset += rig.springVelocity * step;
    }

    // Hard stops as a backstop (a bounce can overshoot the soft limits); hitting one kills the
    // velocity into it rather than letting the camera stick there.
    const auto stop = [](float& offset, float& velocity, float low, float high)
    {
        if (offset < low) { offset = low; velocity = std::max(0.f, velocity); }
        if (offset > high) { offset = high; velocity = std::min(0.f, velocity); }
    };

    stop(rig.springOffset.x, rig.springVelocity.x, -settings.maxSway, settings.maxSway);
    stop(rig.springOffset.y, rig.springVelocity.y, -settings.maxSway, settings.maxSway);
    stop(rig.springOffset.z, rig.springVelocity.z, -settings.maxStretch, settings.maxCompress);
}

/**
 * Chase camera: behind and above the ship in the ship's own frame, displaced by the spring, and
 * looking along the nose tilted down by lookDownDegrees. Because the camera pitches with the
 * ship, the ship keeps its place on screen whether flying level, climbing or diving.
 */
inline void updateCameraToFollowShip(
    Camera& camera,
    const Ship& ship,
    float dt,
    ShipCameraRig& rig,
    const ShipCameraSettings& settings = {}
)
{
    updateCameraSpring(ship, dt, rig, settings);

    const Vec3 right = shipUnrolledRight(ship);
    const Vec3 up = shipUnrolledUp(ship);
    const Vec3 forward = shipForward(ship);

    // Stretch and compression slide the camera along its boom (the line from the ship to the
    // rest position), so the ship stays put on screen and only its apparent size changes. Sway
    // is added on top, in the ship's right/up axes.
    const float boomScale = settings.followDistance > 0.f
        ? std::max(0.1f, (settings.followDistance - rig.springOffset.z) / settings.followDistance)
        : 1.f;

    const Vec3 offset =
    {
        rig.springOffset.x,
        settings.followHeight * boomScale + rig.springOffset.y,
        -settings.followDistance * boomScale
    };

    camera.position = ship.position + right * offset.x + up * offset.y + forward * offset.z;

    constexpr float degreesToRadians = 3.14159265358979323846f / 180.f;
    constexpr float maxCameraPitch = 1.5f;

    camera.yaw = ship.yaw;
    camera.pitch = std::clamp(ship.pitch - settings.lookDownDegrees * degreesToRadians, -maxCameraPitch, maxCameraPitch);
    camera.roll = 0.f;

    // Field of view: draws in slightly while the cruise drive charges, then widens with speed
    // once it engages, so the view itself conveys the jump to cruise.
    float targetOffset = 0.f;

    if (cruiseCharging(ship))
    {
        targetOffset = -settings.chargeFovPull * ship.cruiseCharge;
    }
    else if (ship.cruiseMaxSpeed > ship.maxSpeed)
    {
        const float cruiseFraction = std::clamp(
            (length(ship.velocity) - ship.maxSpeed) / (ship.cruiseMaxSpeed - ship.maxSpeed),
            0.f,
            1.f
        );
        targetOffset = settings.cruiseFovBoost * std::sqrt(cruiseFraction);
    }

    if (dt > 0.f)
        rig.fovOffset += (targetOffset - rig.fovOffset) * (1.f - std::exp(-settings.fovResponse * dt));

    camera.fov = settings.baseFov + rig.fovOffset + rig.sceneFovOffset;
}

/** Orbits the camera around the ship to show off its wireframe model. */
inline void updateCameraToShowcaseShip(
    Camera& camera,
    const Ship& ship,
    float dt,
    ShipCameraRig& rig,
    const ShipCameraSettings& settings = {}
)
{
    camera.fov = settings.baseFov;

    rig.showcaseAngle = wrapAngle(rig.showcaseAngle + settings.showcaseSpeed * dt);

    const Vec3 worldUp = {0.f, 1.f, 0.f};
    const Vec3 orbitOffset =
    {
        std::sin(rig.showcaseAngle) * settings.showcaseDistance,
        settings.showcaseHeight,
        std::cos(rig.showcaseAngle) * settings.showcaseDistance
    };

    camera.position = ship.position + orbitOffset;
    pointCameraAt(camera, ship.position + worldUp * 80.f);
}

/** Chooses between the normal chase camera and the Up Arrow showcase orbit. */
inline void updateShipCamera(
    Camera& camera,
    const Ship& ship,
    float dt,
    ShipCameraRig& rig,
    const ShipCameraSettings& settings = {}
)
{
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
    {
        updateCameraToShowcaseShip(camera, ship, dt, rig, settings);

        // Keep the spring's velocity history current, so letting go of Up doesn't cause a lurch.
        rig.previousShipVelocity = ship.velocity;
        return;
    }

    rig.showcaseAngle = ship.yaw;
    updateCameraToFollowShip(camera, ship, dt, rig, settings);
}

#endif //DUSK_SHIP_CONTROLLER_H
