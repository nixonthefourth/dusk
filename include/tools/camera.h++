//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_CAMERA_H
#define DUSK_CAMERA_H

#include "../math/Vec3.h++"

/** Camera state used by input, view-matrix creation, and projection. */
struct Camera {
    /** Camera origin in world space. */
    Vec3 position;

    /** Horizontal rotation in radians. */
    float yaw = 0.f;

    /** Vertical rotation in radians. */
    float pitch = 0.f;

    /** Roll rotation in radians. */
    float roll = 0.f;

    /** Movement speed in world units per second. */
    float speed = 800.f;

    /** Vertical field-of-view in degrees. */
    float fov = 90.f;
};

#endif //DUSK_CAMERA_H
