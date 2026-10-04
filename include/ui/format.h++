//
// Small text-formatting helpers shared by the HUD and the maps.
//

#ifndef DUSK_FORMAT_H
#define DUSK_FORMAT_H

#include <cstdio>
#include <string>

/** Compact distance readout in world units: 950, 12.4K, 1.32M. */
inline std::string formatWorldDistance(float distance)
{
    char buffer[32];
    const float magnitude = distance < 0.f ? -distance : distance;

    if (magnitude >= 1000000.f)
        std::snprintf(buffer, sizeof(buffer), "%.2fM", distance / 1000000.f);
    else if (magnitude >= 1000.f)
        std::snprintf(buffer, sizeof(buffer), "%.1fK", distance / 1000.f);
    else
        std::snprintf(buffer, sizeof(buffer), "%.0f", distance);

    return buffer;
}

/** Whole-number readout with an explicit sign, e.g. "+12" or "-4". */
inline std::string formatSigned(float value)
{
    char buffer[16];

    // Anything that rounds to zero reads as a plain "0", never "+0" or "-0".
    if (value > -0.5f && value < 0.5f)
        return "0";

    std::snprintf(buffer, sizeof(buffer), "%+.0f", value);
    return buffer;
}

/** Three-digit heading, e.g. "047". */
inline std::string formatHeading(float degrees)
{
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "%03d", static_cast<int>(degrees + 0.5f) % 360);
    return buffer;
}

#endif //DUSK_FORMAT_H
