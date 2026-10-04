//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_SHIP_CONTROLLER_H
#define DUSK_SHIP_CONTROLLER_H

#include "objects/ship.h++"
#include "systems/ship_physics.h++"
#include "tools/camera.h++"
#include <SFML/Window/Keyboard.hpp>
#include <algorithm>
#include <cmath>

/** Third-person camera offset relative to the ship. */
struct ShipCameraSettings {
    /** Distance behind the ship along its local forward axis. */
    float followDistance = 500.f;

    /** Height above the ship in world space, keeping the view upright. */
    float followHeight = 500.f;

    /** Point ahead of the ship that the camera looks toward. */
    float lookAhead = 700.f;

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

    // Turn demands; the physics step spins the ship toward these rates and stops it on release.
    ship.yawInput = keyAxis(sf::Keyboard::Key::A, sf::Keyboard::Key::D);
    ship.pitchInput = keyAxis(sf::Keyboard::Key::E, sf::Keyboard::Key::Q);
    ship.precisionInput =
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);

    if (keyPressedThisFrame(sf::Keyboard::Key::Down, inputState.reverseToggleWasDown) && !ship.cruiseEngaged)
    {
        ship.reverseThrust = !ship.reverseThrust;
        ship.throttle = 0.f;
    }

    if (keyPressedThisFrame(sf::Keyboard::Key::F, inputState.assistToggleWasDown))
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
        if (ship.cruiseEngaged)
            disengageCruise(ship);
        else
            engageCruise(ship); // refused while mass-locked; the HUD shows why
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

/** Places the camera slightly above and behind the ship, looking forward over it. */
inline void updateCameraToFollowShip(
    Camera& camera,
    const Ship& ship,
    const ShipCameraSettings& settings = {}
)
{
    const Vec3 forward = shipForward(ship);
    const Vec3 worldUp = {0.f, 1.f, 0.f};
    const Vec3 target = ship.position + forward * settings.lookAhead;

    camera.position =
        ship.position -
        forward * settings.followDistance +
        worldUp * settings.followHeight;

    pointCameraAt(camera, target);
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
        return;
    }

    rig.showcaseAngle = ship.yaw;
    updateCameraToFollowShip(camera, ship, settings);
}

#endif //DUSK_SHIP_CONTROLLER_H
