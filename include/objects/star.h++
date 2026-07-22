//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_STAR_H
#define DUSK_STAR_H

#include "math/Vec3.h++"

/** A single world-space point rendered as a projected star. */
struct Star {
    /** Position in the recycled starfield volume. */
    Vec3 position;
};

#endif //DUSK_STAR_H
