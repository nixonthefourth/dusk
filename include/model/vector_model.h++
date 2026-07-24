//
// Created by Mykyta Khomiakov on 24/07/2026.
//

#ifndef DUSK_VECTOR_MODEL_H
#define DUSK_VECTOR_MODEL_H

#include "math/Vec3.h++"
#include <vector>

/** One triangle face in a local-space vector model. Winding is expected to face outward. */
struct VectorFace {
    int a = 0;
    int b = 0;
    int c = 0;
};

/** One line segment in a local-space vector model. */
struct VectorLine {
    int start = 0;
    int end = 0;

    /** Optional visibility hint used by hand-authored models such as the ship underside. */
    bool hideWhenViewedFromAbove = false;
};

/** A wire/vector model made of local-space vertices and line edges. */
struct VectorModel {
    std::vector<Vec3> vertices;
    std::vector<VectorLine> lines;
    std::vector<VectorFace> faces;
};

#endif //DUSK_VECTOR_MODEL_H
