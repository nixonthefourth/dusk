//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_SHIP_CONTROLLER_H
#define DUSK_SHIP_CONTROLLER_H

#include "objects/ship.h++"
#include "tools/camera.h++"
#include <SFML/Window/Keyboard.hpp>
#include <algorithm>
#include <cmath>

/** Third-person camera offset relative to the ship. */
struct ShipCameraSettings {
    /** Distance behind the ship along its local forward axis. */
    float followDistance = 1000.f;

    /** Height above the ship in world space, keeping the view upright. */
    float followHeight = 800.f;

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

/** Applies keyboard input to the ship before the camera follows it. */
inline void updateShipFromKeyboard(Ship& ship, float dt)
{
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W))
        ship.position += shipForward(ship) * ship.speed * dt;

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S))
        ship.position -= shipForward(ship) * ship.speed * dt;

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
        ship.yaw -= ship.yawSpeed * dt;

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
        ship.yaw += ship.yawSpeed * dt;

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q))
        ship.pitch += ship.pitchSpeed * dt;

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E))
        ship.pitch -= ship.pitchSpeed * dt;

    clampShipPitch(ship);
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
