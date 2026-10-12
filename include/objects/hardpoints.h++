//
// Hardpoints: things mounted on the hull that fire. The pilot selects one group at a time and
// fires it with Space; R cycles through whatever is fitted (Commander::hardpoints()).
//
// The mining laser cuts ore (systems/mining.h++); the four weapons (systems/combat.h++) damage
// ships: a fixed pulse laser, a fixed beam laser, a rocket launcher whose rockets home on the
// locked target, and a turret laser that aims itself at it.
//

#ifndef DUSK_HARDPOINTS_H
#define DUSK_HARDPOINTS_H

#include "math/Vec3.h++"
#include <array>

/** Rockets the launcher holds when full. */
constexpr int rocketMagazine = 12;

/** What a hardpoint group is. */
enum class HardpointKind { MiningLaser, PulseLaser, BeamLaser, RocketLauncher, TurretLaser };

/** A hardpoint group's name, as shown on the HUD. */
inline const char* hardpointName(HardpointKind kind)
{
    switch (kind)
    {
        case HardpointKind::MiningLaser: return "MINING LASER";
        case HardpointKind::PulseLaser: return "PULSE LASER";
        case HardpointKind::BeamLaser: return "BEAM LASER";
        case HardpointKind::RocketLauncher: return "ROCKET LAUNCHER";
        case HardpointKind::TurretLaser: return "TURRET LASER";
    }

    return "?";
}

/** True for the groups that damage ships (everything but the mining laser). */
inline bool isWeapon(HardpointKind kind)
{
    return kind != HardpointKind::MiningLaser;
}

/**
 * Where a group's muzzles sit on the ship, in its local frame (x right, y up, z forward; the ship
 * is about 500 wide, 300 long and 100 thick). Pairs fire alternately or together; the turret has
 * one mount on top of the hull, given twice.
 */
inline std::array<Vec3, 2> hardpointMuzzles(HardpointKind kind)
{
    switch (kind)
    {
        case HardpointKind::MiningLaser: return {Vec3{-70.f, -38.f, 128.f}, Vec3{70.f, -38.f, 128.f}};
        case HardpointKind::PulseLaser: return {Vec3{-150.f, 0.f, 95.f}, Vec3{150.f, 0.f, 95.f}};
        case HardpointKind::BeamLaser: return {Vec3{-34.f, -22.f, 148.f}, Vec3{34.f, -22.f, 148.f}};
        case HardpointKind::RocketLauncher: return {Vec3{-205.f, -24.f, 30.f}, Vec3{205.f, -24.f, 30.f}};
        case HardpointKind::TurretLaser: return {Vec3{0.f, 60.f, 10.f}, Vec3{0.f, 60.f, 10.f}};
    }

    return {};
}

#endif //DUSK_HARDPOINTS_H
