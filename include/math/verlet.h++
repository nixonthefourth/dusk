//
// Created by Mykyta Khomiakov on 24/07/2026.
//

#ifndef DUSK_VERLET_H
#define DUSK_VERLET_H

#include "Vec3.h++"
#include <cmath>

namespace verlet {

    /// @brief Updates position vector of an object
    /// @param r Current displacement
    /// @param v Current velocity
    /// @param a Given acceleration
    /// @param dt Current timestep
    /// @return Returns updated displacement
    Vec3 position_update(const Vec3& r, const Vec3& v, const Vec3& a, float dt) {
        return r + v * dt + 0.5 * a * dt * dt;
    }

    /// @brief Updates velocity vector of an object
    /// @param v Current velocity
    /// @param a_old Initial acceleration
    /// @param a_new Acceleration dt+1
    /// @param dt Current timestep
    /// @return Returns updated velocity
    Vec3 velocity_update(const Vec3& v, const Vec3& a_old, const Vec3& a_new, float dt) {
        return v + 0.5 * (a_old + a_new) * dt;
    }
}

#endif //DUSK_VERLET_H
