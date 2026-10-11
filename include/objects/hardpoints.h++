//
// Hardpoints: things mounted on the hull that fire. The pilot selects one group at a time and
// fires it; the hardpoint key cycles through whatever is fitted.
//
// Only the mining laser exists so far. The other kinds are listed so that weapons can slot into
// the same selection and firing path later without changing the controls.
//

#ifndef DUSK_HARDPOINTS_H
#define DUSK_HARDPOINTS_H

#include "math/Vec3.h++"
#include <array>

/** What a hardpoint group is. Only MiningLaser can be fitted yet; the others are reserved for weapons. */
enum class HardpointKind { MiningLaser, PulseLaser, RocketPod };

/** A hardpoint group's name, as shown on the HUD. */
inline const char* hardpointName(HardpointKind kind)
{
    switch (kind)
    {
        case HardpointKind::MiningLaser: return "MINING LASER";
        case HardpointKind::PulseLaser: return "PULSE LASER";
        case HardpointKind::RocketPod: return "ROCKET POD";
    }

    return "?";
}

/**
 * Where a group's muzzles sit on the ship, in its local frame (x right, y up, z forward). The
 * mining lasers are a pair under the nose, either side of the centre line.
 */
inline std::array<Vec3, 2> hardpointMuzzles(HardpointKind kind)
{
    switch (kind)
    {
        case HardpointKind::MiningLaser: return {Vec3{-70.f, -38.f, 128.f}, Vec3{70.f, -38.f, 128.f}};
        case HardpointKind::PulseLaser: return {Vec3{-150.f, 0.f, 90.f}, Vec3{150.f, 0.f, 90.f}};
        case HardpointKind::RocketPod: return {Vec3{-200.f, -20.f, 40.f}, Vec3{200.f, -20.f, 40.f}};
    }

    return {};
}

#endif //DUSK_HARDPOINTS_H
