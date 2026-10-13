//
// Missions: procedurally generated jobs on every station's board, and what happens to the ones a
// commander takes.
//
// A board is generated, not stored: the galaxy seed, the system and the galaxy clock (in half-hour
// steps) seed it, so the same station shows the same board until it refreshes, and boards cost
// nothing to keep. Missions already taken are left off it. What a board offers follows the system:
// mining systems post more mining contracts, poor systems more bounties, tech systems more courier
// runs and farming systems more deliveries.
//
// Taken missions live in the Commander. They are completed on docking at the right station (any
// station, for a bounty whose targets are all destroyed), and fail if their deadline passes, or,
// for courier data and delivery cargo, if the ship carrying them is destroyed.
//

#ifndef DUSK_MISSIONS_H
#define DUSK_MISSIONS_H

#include "objects/commander.h++"
#include "objects/mission.h++"
#include "objects/mining_hatch.h++"
#include "procgen/galaxy.h++"
#include "systems/trading.h++"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace missions {

    /* ---- Tuning ------------------------------------------------------------------------------ */

    /** A board refreshes every this many seconds of galaxy time. */
    constexpr double boardRefreshSeconds = 1800.0;

    /** How many missions a board offers (inclusive range). */
    constexpr int boardMinimum = 4;
    constexpr int boardMaximum = 7;

    /** Most missions a commander can have at once. */
    constexpr int maxActiveMissions = 5;

    /** Destinations are other stations within this many light years. */
    constexpr float destinationRange = 22.f;

    /** Every deadline allows this long for flying in-system and docking, on top of the jump. */
    constexpr double deadlineAllowance = 1500.0;

    /** Deadlines allow this many times the jump's own galaxy time, for refuelling stops and detours. */
    constexpr double jumpTimeMargin = 3.0;

    /** What a tonne of each resource is worth to a mining contract. */
    inline double resourceContractPrice(MiningResource resource)
    {
        switch (resource)
        {
            case MiningResource::Iron: return 60.0;
            case MiningResource::Silicates: return 50.0;
            case MiningResource::Nickel: return 90.0;
            case MiningResource::WaterIce: return 70.0;
            case MiningResource::Cobalt: return 160.0;
            case MiningResource::Platinum: return 420.0;
        }

        return 60.0;
    }

    /* ---- Names ------------------------------------------------------------------------------- */

    /** Picks one entry of a fixed list. */
    template <std::size_t N>
    const char* pick(std::mt19937& rng, const std::array<const char*, N>& list)
    {
        return list[std::uniform_int_distribution<std::size_t>(0, N - 1)(rng)];
    }

    /** A client's name: a company or a person, from syllables and stock words. */
    inline std::string clientName(std::mt19937& rng)
    {
        static const std::array<const char*, 12> firstParts = {"KAR", "VEL", "OST", "MIR", "TAL", "QUE", "DRO", "ISA", "BEN", "SOL", "ZAR", "LUM"};
        static const std::array<const char*, 10> secondParts = {"ANE", "OVA", "IST", "ERRA", "ONIS", "AKI", "ETH", "UMA", "ORIX", "ADE"};
        static const std::array<const char*, 8> companies = {"FREIGHT", "HOLDINGS", "MINING CO.", "LOGISTICS", "SURVEY GUILD", "TRADING HOUSE", "SHIPPING", "SYNDICATE"};

        std::string name = std::string(pick(rng, firstParts)) + pick(rng, secondParts);

        if (std::uniform_real_distribution<float>(0.f, 1.f)(rng) < 0.55f)
            return name + " " + pick(rng, companies);

        static const std::array<const char*, 4> titles = {"CAPTAIN", "DR", "AGENT", "ELDER"};
        return std::string(pick(rng, titles)) + " " + name;
    }

    /** What a courier carries. */
    inline std::string courierPackage(std::mt19937& rng)
    {
        static const std::array<const char*, 10> packages =
        {
            "encrypted survey data", "a sealed diplomatic dispatch", "medical research files", "a bank transfer key",
            "navigation charts", "a legal deposition", "classified engineering plans", "a private letter",
            "insurance records", "a union ballot"
        };
        return pick(rng, packages);
    }

    /** What a delivery carries. */
    inline std::string deliveryCargo(std::mt19937& rng)
    {
        static const std::array<const char*, 10> cargoes =
        {
            "medical supplies", "machine parts", "water purifiers", "reactor coolant", "seed stock",
            "spare hull plating", "colony equipment", "atmosphere processors", "relief rations", "survey drones"
        };
        return pick(rng, cargoes);
    }

    /** A pirate gang's name. */
    inline std::string gangName(std::mt19937& rng)
    {
        static const std::array<const char*, 10> adjectives = {"CRIMSON", "HOLLOW", "IRON", "BLACK", "SILENT", "RUSTED", "LAST", "BROKEN", "GREY", "BURNING"};
        static const std::array<const char*, 10> nouns = {"WAKE", "TALONS", "JACKALS", "COMET", "LANTERNS", "REAVERS", "TIDE", "VULTURES", "KNIVES", "SAINTS"};
        return std::string("THE ") + pick(rng, adjectives) + " " + pick(rng, nouns);
    }

    /* ---- Geometry and time ------------------------------------------------------------------- */

    /** Light years between two systems. */
    inline float distanceBetween(const Galaxy& galaxy, int a, int b)
    {
        return galacticDistance(galaxy.systems[static_cast<std::size_t>(a)], galaxy.systems[static_cast<std::size_t>(b)]);
    }

    /** Galaxy-clock seconds to allow for a trip of `lightYears`: the jumps, with margin, plus time to fly in and dock. */
    inline double timeAllowance(float lightYears)
    {
        return static_cast<double>(lightYears) * trading::jumpSecondsPerLightYear * jumpTimeMargin + deadlineAllowance;
    }

    /** "1:23:40" or "23:40": the time left until `deadline`, or "EXPIRED". */
    inline std::string timeLeft(double deadline, double now)
    {
        const double remaining = deadline - now;

        if (remaining <= 0.0)
            return "EXPIRED";

        const int seconds = static_cast<int>(remaining);
        char buffer[32];

        if (seconds >= 3600)
            std::snprintf(buffer, sizeof(buffer), "%d:%02d:%02d", seconds / 3600, (seconds / 60) % 60, seconds % 60);
        else
            std::snprintf(buffer, sizeof(buffer), "%d:%02d", seconds / 60, seconds % 60);

        return buffer;
    }

    /* ---- The board --------------------------------------------------------------------------- */

    /** A 64-bit mixer (splitmix64's finaliser). */
    inline std::uint64_t mix(std::uint64_t value)
    {
        value += 0x9E3779B97F4A7C15ull;
        value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
        value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
        return value ^ (value >> 31);
    }

    /** Which half-hour step of the galaxy clock `time` falls in: boards are the same within one. */
    inline std::int64_t boardEpoch(double time)
    {
        return static_cast<std::int64_t>(std::floor(time / boardRefreshSeconds));
    }

    /** How likely each type is on a board in a system like `info`: relative weights for courier, delivery, mining, bounty. */
    inline std::array<float, 4> typeWeights(const SystemInfo& info)
    {
        std::array<float, 4> weights = {3.f, 3.f, 2.f, 1.5f};

        if (info.occupation == "Mining")
            weights[2] *= 2.2f;
        else if (info.occupation == "Agricultural")
            weights[1] *= 1.6f;
        else
            weights[0] *= 1.6f;

        if (info.economyTier == EconomyTier::Poor)
            weights[3] *= 2.6f;
        else if (info.economyTier == EconomyTier::Progressive)
            weights[3] *= 0.4f;

        return weights;
    }

    /** Stations within destinationRange of `system`, nearest first (not `system` itself). */
    inline std::vector<int> destinationsNear(const Galaxy& galaxy, int system)
    {
        std::vector<std::pair<float, int>> nearby;

        for (int i = 0; i < static_cast<int>(galaxy.systems.size()); ++i)
        {
            if (i == system || galaxy.systems[static_cast<std::size_t>(i)].stationCount <= 0)
                continue;

            const float distance = distanceBetween(galaxy, system, i);

            if (distance <= destinationRange)
                nearby.emplace_back(distance, i);
        }

        std::sort(nearby.begin(), nearby.end());
        std::vector<int> result;

        for (const auto& [distance, index] : nearby)
            result.push_back(index);

        return result;
    }

    /**
     * The mission board at `system` at galaxy time `now`, without any mission whose id is in
     * `taken`. The same inputs always give the same board.
     */
    inline std::vector<Mission> missionBoard(const Galaxy& galaxy, int system, double now, const std::vector<std::uint64_t>& taken)
    {
        std::vector<Mission> board;

        if (system < 0 || system >= static_cast<int>(galaxy.systems.size()) || galaxy.systems[static_cast<std::size_t>(system)].stationCount <= 0)
            return board;

        const SystemInfo& here = galaxy.systems[static_cast<std::size_t>(system)];
        const std::int64_t epoch = boardEpoch(now);
        const std::uint64_t boardSeed = mix(mix(galaxy.seed) ^ (static_cast<std::uint64_t>(system) * 0x9E3779B1ull) ^ (static_cast<std::uint64_t>(epoch) << 20));
        std::mt19937 rng(static_cast<std::uint32_t>(boardSeed ^ (boardSeed >> 32)));
        std::uniform_real_distribution<float> unit(0.f, 1.f);

        const std::vector<int> destinations = destinationsNear(galaxy, system);
        const std::array<float, 4> weights = typeWeights(here);
        const int count = std::uniform_int_distribution<int>(boardMinimum, boardMaximum)(rng);

        // Missions on this board expire with it at the latest, so they're all posted at the start of the epoch.
        const double posted = static_cast<double>(epoch) * boardRefreshSeconds;

        for (int index = 0; index < count; ++index)
        {
            Mission mission;
            mission.id = mix(boardSeed + static_cast<std::uint64_t>(index) * 0x632BE59BD9B4E019ull) | 1ull;
            mission.origin = system;
            mission.client = clientName(rng);

            // Pick a type by weight; courier and delivery need somewhere to go.
            const float roll = unit(rng) * (weights[0] + weights[1] + weights[2] + weights[3]);
            MissionType type = roll < weights[0] ? MissionType::Courier
                             : roll < weights[0] + weights[1] ? MissionType::Delivery
                             : roll < weights[0] + weights[1] + weights[2] ? MissionType::Mining
                             : MissionType::Bounty;

            if (destinations.empty() && (type == MissionType::Courier || type == MissionType::Delivery))
                type = MissionType::Mining;

            mission.type = type;

            // Somewhere to go: a random station in range (nearer ones a little likelier), or here.
            const auto farDestination = [&]()
            {
                const float r = unit(rng);
                const std::size_t pickIndex = static_cast<std::size_t>(r * r * static_cast<float>(destinations.size()));
                return destinations[std::min(pickIndex, destinations.size() - 1)];
            };

            switch (type)
            {
                case MissionType::Courier:
                {
                    mission.destination = farDestination();
                    mission.cargoName = courierPackage(rng);
                    const float distance = distanceBetween(galaxy, system, mission.destination);
                    mission.reward = std::round(120.0 + 22.0 * distance + 40.0 * unit(rng));
                    mission.deadline = posted + timeAllowance(distance);
                    break;
                }

                case MissionType::Delivery:
                {
                    mission.destination = farDestination();
                    mission.cargoName = deliveryCargo(rng);
                    mission.cargoTonnes = std::uniform_int_distribution<int>(2, 8)(rng);
                    const float distance = distanceBetween(galaxy, system, mission.destination);
                    mission.reward = std::round(150.0 + 14.0 * mission.cargoTonnes + 24.0 * distance + 50.0 * unit(rng));
                    mission.deadline = posted + timeAllowance(distance);
                    break;
                }

                case MissionType::Mining:
                {
                    // Usually wanted here; sometimes at a station nearby.
                    mission.destination = destinations.empty() || unit(rng) < 0.6f ? system : farDestination();
                    mission.resource = allMiningResources[std::min<std::size_t>(miningResourceCount - 1, static_cast<std::size_t>(unit(rng) * unit(rng) * miningResourceCount))];
                    const float most = mission.resource == MiningResource::Platinum ? 1.5f : (mission.resource == MiningResource::Cobalt ? 3.f : 5.f);
                    mission.resourceTonnes = std::round((0.5f + unit(rng) * (most - 0.5f)) * 2.f) * 0.5f;
                    const float distance = distanceBetween(galaxy, system, mission.destination);
                    mission.reward = std::round(60.0 + mission.resourceTonnes * resourceContractPrice(mission.resource) * 1.3 + 18.0 * distance);
                    mission.deadline = posted + timeAllowance(distance) + 2400.0;
                    break;
                }

                case MissionType::Bounty:
                {
                    // The gang is in this system, or a nearby one.
                    mission.destination = destinations.empty() || unit(rng) < 0.55f ? system : farDestination();
                    mission.gangName = gangName(rng);
                    mission.pirateCount = std::uniform_int_distribution<int>(1, 3)(rng);
                    mission.piratesLeft = mission.pirateCount;
                    const float distance = distanceBetween(galaxy, system, mission.destination);
                    mission.reward = std::round(350.0 * mission.pirateCount + 20.0 * distance + 80.0 * unit(rng));
                    mission.deadline = posted + timeAllowance(distance) + 2400.0;
                    break;
                }
            }

            // A mission that would already have run out is no use: leave it off.
            if (mission.deadline <= now + 300.0)
                continue;

            if (std::find(taken.begin(), taken.end(), mission.id) != taken.end())
                continue;

            board.push_back(mission);
        }

        return board;
    }

    /* ---- Text -------------------------------------------------------------------------------- */

    /** A system's name. */
    inline const std::string& systemName(const Galaxy& galaxy, int system)
    {
        return galaxy.systems[static_cast<std::size_t>(system)].name;
    }

    /** A one-line title: "COURIER   KAVURA PRIME  12.4 LY". */
    inline std::string missionTitle(const Mission& mission, const Galaxy& galaxy)
    {
        char buffer[160];
        const float distance = distanceBetween(galaxy, mission.origin, mission.destination);

        if (mission.destination == mission.origin)
            std::snprintf(buffer, sizeof(buffer), "%s   %s (HERE)", missionTypeName(mission.type), systemName(galaxy, mission.destination).c_str());
        else
            std::snprintf(buffer, sizeof(buffer), "%s   %s  %.1f LY", missionTypeName(mission.type), systemName(galaxy, mission.destination).c_str(), static_cast<double>(distance));

        return buffer;
    }

    /** A sentence describing the job, for the board. */
    inline std::string missionDescription(const Mission& mission, const Galaxy& galaxy)
    {
        const std::string& where = systemName(galaxy, mission.destination);
        char buffer[320];

        switch (mission.type)
        {
            case MissionType::Courier:
                std::snprintf(buffer, sizeof(buffer), "%s needs %s carried to the station at %s. No hold space needed.", mission.client.c_str(), mission.cargoName.c_str(), where.c_str());
                break;

            case MissionType::Delivery:
                std::snprintf(buffer, sizeof(buffer), "%s needs %d t of %s delivered to the station at %s. Takes %d t of hold space.", mission.client.c_str(), mission.cargoTonnes, mission.cargoName.c_str(), where.c_str(), mission.cargoTonnes);
                break;

            case MissionType::Mining:
                std::snprintf(buffer, sizeof(buffer), "%s wants %.1f t of %s brought to the station at %s, from your mining hatch.", mission.client.c_str(), static_cast<double>(mission.resourceTonnes), miningResourceName(mission.resource), where.c_str());
                break;

            case MissionType::Bounty:
                std::snprintf(buffer, sizeof(buffer), "%s posts a bounty on %s: %d ship%s raiding %s. Destroy them, then dock anywhere to collect.", mission.client.c_str(), mission.gangName.c_str(), mission.pirateCount, mission.pirateCount == 1 ? "" : "s", where.c_str());
                break;
        }

        return buffer;
    }

    /** A short progress note for an active mission: what is still to do. */
    inline std::string missionProgress(const Mission& mission, const Galaxy& galaxy, const Commander& commander)
    {
        char buffer[160];
        const std::string& where = systemName(galaxy, mission.destination);

        switch (mission.type)
        {
            case MissionType::Courier:
                std::snprintf(buffer, sizeof(buffer), "CARRY THE DATA TO %s", where.c_str());
                break;

            case MissionType::Delivery:
                std::snprintf(buffer, sizeof(buffer), "DELIVER %d t TO %s", mission.cargoTonnes, where.c_str());
                break;

            case MissionType::Mining:
                std::snprintf(buffer, sizeof(buffer), "BRING %.1f t %s TO %s (HATCH HAS %.1f t)", static_cast<double>(mission.resourceTonnes), miningResourceName(mission.resource), where.c_str(), static_cast<double>(commander.hatch.of(mission.resource)));
                break;

            case MissionType::Bounty:
                if (mission.piratesLeft > 0)
                    std::snprintf(buffer, sizeof(buffer), "DESTROY %d OF %s AT %s", mission.piratesLeft, mission.gangName.c_str(), where.c_str());
                else
                    std::snprintf(buffer, sizeof(buffer), "TARGETS DESTROYED: DOCK ANYWHERE TO COLLECT");
                break;
        }

        return buffer;
    }

    /* ---- Taking, abandoning, completing, failing --------------------------------------------- */

    /** Why `commander` can't take `mission`, or nullptr if they can. */
    inline const char* acceptBlockReason(const Commander& commander, const Mission& mission)
    {
        if (static_cast<int>(commander.missions.size()) >= maxActiveMissions)
            return "MISSION LIMIT REACHED (5)";

        if (mission.type == MissionType::Delivery && commander.cargoRoom() < mission.cargoTonnes)
            return "NOT ENOUGH HOLD SPACE";

        return nullptr;
    }

    /** Takes `mission`, if allowed: it joins the commander's missions and leaves the board. */
    inline bool acceptMission(Commander& commander, const Mission& mission)
    {
        if (acceptBlockReason(commander, mission))
            return false;

        commander.missions.push_back(mission);
        commander.takenMissions.push_back(mission.id);
        return true;
    }

    /** Gives up active mission `index`. Its cargo, if any, goes with it. No penalty beyond the lost reward. */
    inline void abandonMission(Commander& commander, std::size_t index)
    {
        if (index < commander.missions.size())
            commander.missions.erase(commander.missions.begin() + static_cast<std::ptrdiff_t>(index));
    }

    /** What docking somewhere settled: credits paid, and how many missions were completed. */
    struct Settlement {
        double paid = 0.0;
        int completed = 0;
    };

    /**
     * Completes whatever can be completed on docking at `system`: courier and delivery missions
     * to here, mining contracts to here if the hatch holds enough (the ore is handed over), and
     * any bounty whose targets are all destroyed. Pays the rewards.
     */
    inline Settlement completeMissionsAt(Commander& commander, int system)
    {
        Settlement settlement;

        std::erase_if(commander.missions, [&](const Mission& mission)
        {
            bool done = false;

            switch (mission.type)
            {
                case MissionType::Courier:
                case MissionType::Delivery:
                    done = mission.destination == system;
                    break;

                case MissionType::Mining:
                    if (mission.destination == system && commander.hatch.of(mission.resource) + 1e-4f >= mission.resourceTonnes)
                    {
                        commander.hatch.tonnes[static_cast<std::size_t>(mission.resource)] = std::max(0.f, commander.hatch.of(mission.resource) - mission.resourceTonnes);
                        done = true;
                    }
                    break;

                case MissionType::Bounty:
                    done = mission.piratesLeft <= 0;
                    break;
            }

            if (done)
            {
                commander.credits += mission.reward;
                settlement.paid += mission.reward;
                ++settlement.completed;
            }

            return done;
        });

        return settlement;
    }

    /** Fails every mission whose deadline has passed; returns how many failed. */
    inline int failExpired(Commander& commander, double now)
    {
        const std::size_t before = commander.missions.size();
        std::erase_if(commander.missions, [now](const Mission& mission) { return mission.deadline <= now; });
        return static_cast<int>(before - commander.missions.size());
    }

    /** The ship is lost: courier data and delivery cargo go with it. Returns how many missions failed. */
    inline int failCarriedMissions(Commander& commander)
    {
        const std::size_t before = commander.missions.size();
        std::erase_if(commander.missions, [](const Mission& mission) { return mission.carriedInShip(); });
        return static_cast<int>(before - commander.missions.size());
    }

    /** Records a destroyed bounty target: the mission it belongs to has one fewer ship left. */
    inline void recordBountyKill(Commander& commander, std::uint64_t missionId)
    {
        for (Mission& mission : commander.missions)
        {
            if (mission.id == missionId && mission.type == MissionType::Bounty)
                mission.piratesLeft = std::max(0, mission.piratesLeft - 1);
        }
    }

    /** Forgets taken-mission ids from boards that have refreshed since (they can't come back anyway), keeping the list short. */
    inline void pruneTaken(Commander& commander, const Galaxy& galaxy, double now)
    {
        // A taken mission's id can only appear on a board of the same epoch; keep ids still on an active mission, plus the most recent few.
        std::vector<std::uint64_t> kept;

        for (const Mission& mission : commander.missions)
            kept.push_back(mission.id);

        const std::size_t recent = 40;
        const std::size_t start = commander.takenMissions.size() > recent ? commander.takenMissions.size() - recent : 0;

        for (std::size_t i = start; i < commander.takenMissions.size(); ++i)
        {
            if (std::find(kept.begin(), kept.end(), commander.takenMissions[i]) == kept.end())
                kept.push_back(commander.takenMissions[i]);
        }

        (void)galaxy;
        (void)now;
        commander.takenMissions = kept;
    }

    /* ---- Saving ------------------------------------------------------------------------------ */

    /** A mission as one save-file line value: fields separated by '|' (generated text never contains one). */
    inline std::string encodeMission(const Mission& mission)
    {
        std::ostringstream out;
        out << mission.id << '|' << static_cast<int>(mission.type) << '|' << mission.origin << '|' << mission.destination << '|';
        char number[64];
        std::snprintf(number, sizeof(number), "%.1f|%.0f|", mission.deadline, mission.reward);
        out << number << mission.cargoName << '|' << mission.cargoTonnes << '|' << miningResourceKey(mission.resource) << '|';
        std::snprintf(number, sizeof(number), "%.2f|", static_cast<double>(mission.resourceTonnes));
        out << number << mission.gangName << '|' << mission.pirateCount << '|' << mission.piratesLeft << '|' << mission.client;
        return out.str();
    }

    /** The mission in a save-file line value, if it reads as one. Bad or unknown fields make it unreadable, not wrong. */
    inline std::optional<Mission> decodeMission(const std::string& text)
    {
        std::vector<std::string> fields;
        std::string field;
        std::istringstream in(text);

        while (std::getline(in, field, '|'))
            fields.push_back(field);

        if (fields.size() != 14)
            return std::nullopt;

        Mission mission;

        try
        {
            mission.id = std::stoull(fields[0]);
            const int type = std::stoi(fields[1]);

            if (type < 0 || type > 3)
                return std::nullopt;

            mission.type = static_cast<MissionType>(type);
            mission.origin = std::stoi(fields[2]);
            mission.destination = std::stoi(fields[3]);
            mission.deadline = std::stod(fields[4]);
            mission.reward = std::stod(fields[5]);
            mission.cargoName = fields[6];
            mission.cargoTonnes = std::max(0, std::stoi(fields[7]));
            const auto resource = miningResourceFromKey(fields[8]);

            if (!resource)
                return std::nullopt;

            mission.resource = *resource;
            mission.resourceTonnes = std::max(0.f, std::stof(fields[9]));
            mission.gangName = fields[10];
            mission.pirateCount = std::max(0, std::stoi(fields[11]));
            mission.piratesLeft = std::max(0, std::stoi(fields[12]));
            mission.client = fields[13];
        }
        catch (...)
        {
            return std::nullopt;
        }

        return mission;
    }

} // namespace missions

#endif //DUSK_MISSIONS_H
