//
// Created by Mykyta Khomiakov on 24/07/2026.
//

#ifndef DUSK_PLANET_H
#define DUSK_PLANET_H

#include "math/Vec3.h++"

/** A simple spherical world body rendered through the fake-3D projector. */
struct Planet {
    /** Center of the planet in world space. */
    Vec3 position;

    /** World-space sphere radius. */
    float radius = 1000.f;

    /** Whether a projected ellipse ring should be drawn around the planet. */
    bool hasRing = false;

    /** Ring rotation in screen-space degrees. */
    float ringRotationDegrees = 0.f;

    /** Ring height as a fraction of its projected width. */
    float ringFlattening = 0.28f;
};

#endif //DUSK_PLANET_H
