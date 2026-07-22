//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_CAMERA_H
#define DUSK_CAMERA_H

#include "../math/Vec3.h++"

struct Camera {
    Vec3 position;

    // Orientation is stored in radians. Field-of-view is stored in degrees.
    float yaw = 0.f;
    float pitch = 0.f;
    float roll = 0.f;

    float speed = 800.f;

    float fov = 90.f;
};

#endif //DUSK_CAMERA_H
