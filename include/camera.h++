//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_CAMERA_H
#define DUSK_CAMERA_H
#include <SFML/Graphics.hpp>

struct Camera {
    sf::Vector3f position;
    const float speed = 500;
};

#endif //DUSK_CAMERA_H
