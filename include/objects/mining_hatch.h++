//
// Mined resources and the mining hatch that stores them.
//
// Mining yields raw resources, not market goods: they are kept apart from the cargo hold, in a
// small hatch that comes with the mining module, and are meant for missions (which are still to
// come). Their mass counts towards the ship's, like cargo.
//

#ifndef DUSK_MINING_HATCH_H
#define DUSK_MINING_HATCH_H

#include <algorithm>
#include <array>
#include <optional>
#include <string>

/** The resources a mining laser can cut from a rock, commonest first. */
enum class MiningResource { Iron, Silicates, Nickel, WaterIce, Cobalt, Platinum };

/** How many kinds of resource there are. */
constexpr int miningResourceCount = 6;

/** Capacity of the mining hatch, in tonnes. */
constexpr float miningHatchCapacity = 5.f;

/** Every resource, in display order. */
inline constexpr std::array<MiningResource, miningResourceCount> allMiningResources =
{
    MiningResource::Iron, MiningResource::Silicates, MiningResource::Nickel,
    MiningResource::WaterIce, MiningResource::Cobalt, MiningResource::Platinum
};

/** A resource's name as shown on screen. */
inline const char* miningResourceName(MiningResource resource)
{
    switch (resource)
    {
        case MiningResource::Iron: return "IRON ORE";
        case MiningResource::Silicates: return "SILICATES";
        case MiningResource::Nickel: return "NICKEL";
        case MiningResource::WaterIce: return "WATER ICE";
        case MiningResource::Cobalt: return "COBALT";
        case MiningResource::Platinum: return "PLATINUM";
    }

    return "?";
}

/** A resource's key in save files (stable: never rename one). */
inline const char* miningResourceKey(MiningResource resource)
{
    switch (resource)
    {
        case MiningResource::Iron: return "Iron";
        case MiningResource::Silicates: return "Silicates";
        case MiningResource::Nickel: return "Nickel";
        case MiningResource::WaterIce: return "WaterIce";
        case MiningResource::Cobalt: return "Cobalt";
        case MiningResource::Platinum: return "Platinum";
    }

    return "?";
}

/** The resource with save-file key `key`, if there is one. */
inline std::optional<MiningResource> miningResourceFromKey(const std::string& key)
{
    for (MiningResource resource : allMiningResources)
    {
        if (key == miningResourceKey(resource))
            return resource;
    }

    return std::nullopt;
}

/**
 * How common each resource is in rocks, as a relative weight: iron is everywhere, platinum is
 * rare. Used to pick a rock's main and secondary resources.
 */
inline float miningResourceWeight(MiningResource resource)
{
    switch (resource)
    {
        case MiningResource::Iron: return 30.f;
        case MiningResource::Silicates: return 24.f;
        case MiningResource::Nickel: return 18.f;
        case MiningResource::WaterIce: return 16.f;
        case MiningResource::Cobalt: return 9.f;
        case MiningResource::Platinum: return 3.f;
    }

    return 0.f;
}

/** The mining hatch: tonnes of each resource held. Separate from the cargo hold. */
struct MiningHatch {
    std::array<float, miningResourceCount> tonnes{};

    /** Tonnes of one resource. */
    float of(MiningResource resource) const
    {
        return tonnes[static_cast<std::size_t>(resource)];
    }

    /** Adds tonnes of a resource. The caller is responsible for keeping within capacity. */
    void add(MiningResource resource, float amount)
    {
        tonnes[static_cast<std::size_t>(resource)] += std::max(0.f, amount);
    }

    /** Everything in the hatch, in tonnes. */
    float total() const
    {
        float sum = 0.f;

        for (float amount : tonnes)
            sum += amount;

        return sum;
    }
};

#endif //DUSK_MINING_HATCH_H
