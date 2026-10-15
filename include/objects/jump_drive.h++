//
// The jump drive: how far one hyperspace jump can reach, and how much fuel it burns per light
// year. The stock drive is fitted to every ship; the Mk1 and Mk2 are upgrades (see upgrades.h++)
// that reach further and burn less.
//

#ifndef DUSK_JUMP_DRIVE_H
#define DUSK_JUMP_DRIVE_H

#include <algorithm>

/** The longest single jump of jump drive class `drive` (0 stock, 1 Mk1, 2 Mk2), in light years. */
inline float jumpDriveRange(int drive)
{
    switch (std::clamp(drive, 0, 2))
    {
        case 1: return 28.f;
        case 2: return 40.f;
        default: return 18.f;
    }
}

/** Fuel the drive burns per light year jumped, in tonnes. */
inline float jumpDriveFuelPerLightYear(int drive)
{
    switch (std::clamp(drive, 0, 2))
    {
        case 1: return 0.12f;
        case 2: return 0.09f;
        default: return 0.15f;
    }
}

/** The drive's name, as shown on screen. */
inline const char* jumpDriveName(int drive)
{
    switch (std::clamp(drive, 0, 2))
    {
        case 1: return "JUMP DRIVE MK1";
        case 2: return "JUMP DRIVE MK2";
        default: return "STOCK JUMP DRIVE";
    }
}

#endif //DUSK_JUMP_DRIVE_H
