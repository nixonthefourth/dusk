//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_CAMERA_CONTROLLER_H
#define DUSK_CAMERA_CONTROLLER_H

#include "tools/camera.h++"
#include <SFML/Window/Keyboard.hpp>
#include <algorithm>
#include <cmath>

struct CameraControls {
    float lookSpeed = 1.6f;
    float sprintMultiplier = 4.f;
};

inline Vec3 cameraForward(const Camera& camera)
{
    const float cosPitch = std::cos(camera.pitch);

    return normalized(
    {
        std::sin(camera.yaw) * cosPitch,
        std::sin(camera.pitch),
        std::cos(camera.yaw) * cosPitch
    });
}

inline Vec3 cameraRight(const Camera& camera)
{
    return normalized(
    {
        std::cos(camera.yaw),
        0.f,
        -std::sin(camera.yaw)
    });
}

inline void updateCameraFromKeyboard(Camera& camera, float dt, const CameraControls& controls = {})
{
    const float speed =
        camera.speed *
        (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ? controls.sprintMultiplier : 1.f);

    const Vec3 forward = cameraForward(camera);
    const Vec3 right = cameraRight(camera);
    const Vec3 up = {0.f, 1.f, 0.f};

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W))
        camera.position += forward * speed * dt;

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S))
        camera.position -= forward * speed * dt;

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
        camera.position -= right * speed * dt;

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
        camera.position += right * speed * dt;

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q))
        camera.position += up * speed * dt;

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E))
        camera.position -= up * speed * dt;

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
        camera.yaw -= controls.lookSpeed * dt;

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
        camera.yaw += controls.lookSpeed * dt;

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
        camera.pitch += controls.lookSpeed * dt;

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
        camera.pitch -= controls.lookSpeed * dt;

    constexpr float maxPitch = 1.5f;
    camera.pitch = std::clamp(camera.pitch, -maxPitch, maxPitch);
}

#endif //DUSK_CAMERA_CONTROLLER_H
