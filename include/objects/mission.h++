//
// A mission: a job taken from a station's mission board. Plain data; generation, acceptance,
// completion and saving live in systems/missions.h++.
//

#ifndef DUSK_MISSION_H
#define DUSK_MISSION_H

#include "objects/mining_hatch.h++"
#include <cstdint>
#include <string>

/** The kinds of mission a board can offer. */
enum class MissionType { Courier, Delivery, Mining, Bounty, Defence, Strike };

/** A mission type's name, as shown on the board. */
inline const char* missionTypeName(MissionType type)
{
    switch (type)
    {
        case MissionType::Courier: return "COURIER";
        case MissionType::Delivery: return "DELIVERY";
        case MissionType::Mining: return "MINING CONTRACT";
        case MissionType::Bounty: return "BOUNTY";
        case MissionType::Defence: return "STATION DEFENCE";
        case MissionType::Strike: return "OUTPOST STRIKE";
    }

    return "?";
}

/**
 * One mission. What matters depends on its type:
 *
 *   Courier    carry `cargoName` (a sealed package of data, no hold space) to `destination`
 *   Delivery   carry `cargoTonnes` of `cargoName` (it takes hold space) to `destination`
 *   Mining     bring `resourceTonnes` of `resource`, from the mining hatch, to `destination`
 *   Bounty     destroy `pirateCount` ships of the `gangName` gang in system `destination`, then
 *              dock anywhere to be paid
 *   Defence    a raid of `pirateCount` ships of `gangName` is coming for the station in system
 *              `destination`: destroy them before the station's `integrity` runs out, then dock there
 *   Strike     destroy the `gangName` gang's outpost in system `destination` (`piratesLeft` is 1
 *              while it stands), then dock anywhere to be paid
 *
 * Every mission must be finished before `deadline` (on the galaxy clock) or it fails.
 */
struct Mission {
    /** Identity, unique per board posting: the board leaves out missions already taken. */
    std::uint64_t id = 0;

    MissionType type = MissionType::Courier;

    /** The system it was posted in, and the one it is to be done in (both indices into Galaxy::systems). */
    int origin = 0;
    int destination = 0;

    /** Galaxy-clock time it must be done by, and what it pays in credits. */
    double deadline = 0.0;
    double reward = 0.0;

    /** Courier and delivery: what is carried, and (delivery only) how many tonnes. */
    std::string cargoName;
    int cargoTonnes = 0;

    /** Mining: what to bring and how much. */
    MiningResource resource = MiningResource::Iron;
    float resourceTonnes = 0.f;

    /** Bounty: the gang, how many ships, and how many are still flying. */
    std::string gangName;
    int pirateCount = 0;
    int piratesLeft = 0;

    /** Defence: how much of the station's defences are left, in percent; at zero the station falls and the mission fails. */
    float integrity = 100.f;

    /** Who posted it (a name for the board). */
    std::string client;

    /** True for missions whose goods ride in the ship and are lost with it (courier data, delivery cargo). */
    bool carriedInShip() const
    {
        return type == MissionType::Courier || type == MissionType::Delivery;
    }
};

#endif //DUSK_MISSION_H
