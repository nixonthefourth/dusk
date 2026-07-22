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

    /** Height above the ship along its local up axis. */
    float followHeight = 800.f;

    /** Point ahead of the ship that the camera looks toward. */
    float lookAhead = 700.f;
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

/** Places the camera slightly above and behind the ship, looking forward over it. */
inline void updateCameraToFollowShip(
    Camera& camera,
    const Ship& ship,
    const ShipCameraSettings& settings = {}
)
{
    const Vec3 forward = shipForward(ship);
    const Vec3 up = shipUp(ship);
    const Vec3 target = ship.position + forward * settings.lookAhead;

    camera.position =
        ship.position -
        forward * settings.followDistance +
        up * settings.followHeight;

    const Vec3 viewDirection = normalized(target - camera.position);
    camera.yaw = std::atan2(viewDirection.x, viewDirection.z);
    camera.pitch = std::asin(std::clamp(viewDirection.y, -1.f, 1.f));
    camera.roll = 0.f;
}

#endif //DUSK_SHIP_CONTROLLER_H
