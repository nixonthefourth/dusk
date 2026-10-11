//
// What the world remembers about mining: how much has been taken from each rock, and what the
// player's mining lasers are doing right now (beam, target, ore fragments in flight).
//
// Belt rocks are not stored, they are regenerated from their belt's seed whenever they are near,
// so each rock is known by its id (see Asteroid::id) and the ledger keeps tonnes taken per id.
// The ledger lives in the World, so it starts empty on entering a system: rocks are full again
// after a jump or a reload.
//

#ifndef DUSK_MINING_STATE_H
#define DUSK_MINING_STATE_H

#include "math/Vec3.h++"
#include "objects/asteroid.h++"
#include "objects/mining_hatch.h++"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace mining {

    /** A rock holds this many tonnes per unit of radius, between a floor and a ceiling. */
    constexpr float tonnesPerUnitRadius = 0.006f;
    constexpr float minimumRockTonnes = 0.3f;
    constexpr float maximumRockTonnes = 14.f;

    /** A rock mined out keeps this fraction of its radius (about a third of its volume). */
    constexpr float minimumRadiusScale = 0.7f;

    /** Share of what a rock yields that is its main resource; the rest is its secondary one. */
    constexpr float mainResourceShare = 0.7f;

    /** Tonnes a rock yields in all, from its (unmined) radius: a 200-unit rock holds 1.2 t, a big one up to 14 t. */
    inline float rockCapacity(const Asteroid& rock)
    {
        return std::clamp(rock.radius * tonnesPerUnitRadius, minimumRockTonnes, maximumRockTonnes);
    }

    /** A 64-bit mixer (splitmix64's finaliser), turning a rock id into well-spread random bits. */
    inline std::uint64_t mixBits(std::uint64_t value)
    {
        value += 0x9E3779B97F4A7C15ull;
        value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
        value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
        return value ^ (value >> 31);
    }

    /** What a rock is made of: a main resource and a different, secondary one. */
    struct RockComposition {
        MiningResource main = MiningResource::Iron;
        MiningResource secondary = MiningResource::Silicates;
    };

    /** Picks a resource by weight from a uniform number in [0, 1), skipping `excluded` if given. */
    inline MiningResource pickResource(double unit, const MiningResource* excluded = nullptr)
    {
        float totalWeight = 0.f;

        for (MiningResource resource : allMiningResources)
        {
            if (!excluded || resource != *excluded)
                totalWeight += miningResourceWeight(resource);
        }

        float threshold = static_cast<float>(unit) * totalWeight;

        for (MiningResource resource : allMiningResources)
        {
            if (excluded && resource == *excluded)
                continue;

            threshold -= miningResourceWeight(resource);

            if (threshold < 0.f)
                return resource;
        }

        return excluded && *excluded == MiningResource::Iron ? MiningResource::Silicates : MiningResource::Iron;
    }

    /** The composition of the rock with id `id`, the same every time: it depends only on the id. */
    inline RockComposition rockComposition(std::uint64_t id)
    {
        const std::uint64_t bits = mixBits(id ^ 0x5EEDA57E0D0Cull);
        const double first = static_cast<double>(bits >> 11) / 9007199254740992.0;
        const double second = static_cast<double>(mixBits(bits) >> 11) / 9007199254740992.0;

        RockComposition composition;
        composition.main = pickResource(first);
        composition.secondary = pickResource(second, &composition.main);
        return composition;
    }

    /** Tonnes taken from each rock in this system so far, by rock id. */
    struct MiningLedger {
        std::unordered_map<std::uint64_t, float> extracted;

        /** Tonnes already taken from the rock with this id. */
        float taken(std::uint64_t id) const
        {
            const auto found = extracted.find(id);
            return found == extracted.end() ? 0.f : found->second;
        }

        /** Tonnes still in `rock` (given as generated, unshrunk). */
        float remaining(const Asteroid& rock) const
        {
            return std::max(0.f, rockCapacity(rock) - taken(rock.id));
        }

        /** Fraction of `rock` still there, from 1 (untouched) to 0 (mined out). */
        float remainingFraction(const Asteroid& rock) const
        {
            const float capacity = rockCapacity(rock);
            return capacity > 0.f ? remaining(rock) / capacity : 0.f;
        }

        /**
         * How much smaller the rock looks: the volume left over (a third remains even when it's
         * mined out), as a scale on its radius. 1 for an untouched rock.
         */
        float radiusScale(const Asteroid& rock) const
        {
            if (rock.id == 0 || extracted.empty())
                return 1.f;

            const float fraction = remainingFraction(rock);

            if (fraction >= 1.f)
                return 1.f;

            const float floorVolume = minimumRadiusScale * minimumRadiusScale * minimumRadiusScale;
            return std::cbrt(floorVolume + (1.f - floorVolume) * fraction);
        }

        /** `rock` as it looks now: the same rock, shrunk by what has been taken from it. */
        Asteroid shrunk(const Asteroid& rock) const
        {
            Asteroid shown = rock;
            shown.radius *= radiusScale(rock);
            return shown;
        }

        /** Records `tonnes` taken from the rock with this id. */
        void record(std::uint64_t id, float tonnes)
        {
            extracted[id] += tonnes;
        }
    };

    /** A lump of ore knocked off a rock, flying into the mining hatch. Its tonnes count only when it arrives. */
    struct MiningFragment {
        Vec3 start;           // where it broke off
        Vec3 startVelocity;   // the rock's velocity, which it keeps at first
        Vec3 kick;            // how far it is flung outward before it is pulled in
        Vec3 position;        // where it is now (for drawing)
        MiningResource resource = MiningResource::Iron;
        float tonnes = 0.f;
        float age = 0.f;
        float duration = 1.2f;
        float spin = 0.f;     // for drawing it tumbling
    };

    /** What the mining laser is doing. */
    enum class LaserStatus { Off, NoTarget, Mining, HatchFull, Depleted };

    /** The player's mining lasers this step: the beam, what it hits, and the ore on its way in. */
    struct MiningState {
        /** True while the lasers fire, and how bright the beam is (it ramps up and down over a moment). */
        bool firing = false;
        float beamPower = 0.f;

        /** Where the beams end: on the rock if they hit one, otherwise at full range. */
        Vec3 beamEnd;
        bool hitting = false;
        Vec3 hitNormal;

        LaserStatus status = LaserStatus::Off;

        /** The rock being mined: its id, what it's made of, and the fraction and tonnes left in it. */
        std::uint64_t targetId = 0;
        RockComposition targetComposition;
        float targetRemainingFraction = 0.f;
        float targetRemainingTonnes = 0.f;

        /** Tonnes cut but not yet broken off as a fragment. */
        float accumulator = 0.f;

        std::vector<MiningFragment> fragments;

        /** State of the small random generator for fragment details (deterministic, for tests). */
        std::uint32_t random = 0x1234567u;

        /** Tonnes in fragments still flying in. */
        float inFlightTonnes() const
        {
            float sum = 0.f;

            for (const MiningFragment& fragment : fragments)
                sum += fragment.tonnes;

            return sum;
        }
    };

} // namespace mining

#endif //DUSK_MINING_STATE_H
