//
// The player's persistent state that isn't part of the ship's physics: name and credits.
// SystemScene owns one Commander and keeps it across hyperspace jumps.
//

#ifndef DUSK_COMMANDER_H
#define DUSK_COMMANDER_H

#include "objects/cargo.h++"
#include "objects/fuel_tank.h++"
#include "objects/hardpoints.h++"
#include "objects/mining_hatch.h++"
#include <optional>
#include <vector>
#include <algorithm>
#include <string>

/**
 * The player, Elite style: a commander with a name and a credit balance. Fuel lives on the Ship
 * (it has mass); money, and later reputation, missions and owned ships, live here.
 */
struct Commander {
    /** A nod to Elite's default commander. */
    std::string name = "JAMES";

    /** Credit balance. Trading earns it; missions will too. */
    double credits = 1000.0;

    /** What the hold carries, and the cargo bay module fitted to it. */
    CargoHold cargo;
    CargoModule cargoModule = CargoModule::None;

    /** The fitted fuel tank module, and whether the ship has a docking computer (a paid upgrade). */
    FuelTankModule fuelTank = FuelTankModule::None;
    bool hasDockingComputer = false;

    /**
     * The scanners, both paid upgrades. Without them the galactic chart shows only each system's
     * occupation: the economics scanner reveals its exports, the political scanner its development.
     */
    bool hasEconomicsScanner = false;
    bool hasPoliticalScanner = false;

    /** The mining module (a paid upgrade): two mining lasers and the mining hatch, and what the hatch holds. */
    bool hasMiningModule = false;
    MiningHatch hatch;

    /** The weapon hardpoints (paid upgrades), and the rockets left in the launcher. */
    bool hasPulseLaser = false;
    bool hasBeamLaser = false;
    bool hasRocketLauncher = false;
    bool hasTurretLaser = false;
    int rockets = 0;

    /** Which fitted hardpoint group is selected, as an index into hardpoints(). */
    int selectedHardpoint = 0;

    /** The hardpoint groups fitted to the ship, in the order the hardpoint key cycles through them. */
    std::vector<HardpointKind> hardpoints() const
    {
        std::vector<HardpointKind> fitted;

        if (hasMiningModule)
            fitted.push_back(HardpointKind::MiningLaser);

        if (hasPulseLaser)
            fitted.push_back(HardpointKind::PulseLaser);

        if (hasBeamLaser)
            fitted.push_back(HardpointKind::BeamLaser);

        if (hasRocketLauncher)
            fitted.push_back(HardpointKind::RocketLauncher);

        if (hasTurretLaser)
            fitted.push_back(HardpointKind::TurretLaser);

        return fitted;
    }

    /** The selected hardpoint group, if anything is fitted. */
    std::optional<HardpointKind> selectedHardpointKind() const
    {
        const auto fitted = hardpoints();

        if (fitted.empty())
            return std::nullopt;

        return fitted[static_cast<std::size_t>(std::clamp(selectedHardpoint, 0, static_cast<int>(fitted.size()) - 1))];
    }

    /** Tank size in tonnes: 6 standard, 10 with a Mk1 tank, 14 with a Mk2 tank. */
    float fuelCapacity() const
    {
        return fuelCapacityFor(fuelTank);
    }

    /** Hold space in tonnes: 10 standard, 15 with a Mk1 bay, 20 with a Mk2 bay. */
    int cargoCapacity() const
    {
        return cargoCapacityFor(cargoModule);
    }

    /** Free hold space in tonnes. */
    int cargoRoom() const
    {
        return std::max(0, cargoCapacity() - cargo.total());
    }
};

#endif //DUSK_COMMANDER_H
