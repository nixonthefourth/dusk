//
// Created by Mykyta Khomiakov on 26/07/2026.
//

#ifndef DUSK_SYSTEM_SCENE_H
#define DUSK_SYSTEM_SCENE_H

#include "io/obj_loader.h++"
#include "procgen/galaxy.h++"
#include "procgen/planet_generation.h++"
#include "scenes/scene.h++"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include "objects/npc_ship.h++"
#include "systems/npc_ai.h++"

/** The one reused scene for every system; regenerated on entry from the galaxy seed. */
class SystemScene : public Scene {
public:
    /** Binds this scene to a galaxy and immediately enters one of its systems. */
    SystemScene(Galaxy& galaxy, int startingSystemIndex)
        : galaxy_(galaxy),
          font_("assets/fonts/Jersey15-Regular.ttf"),
          label_(font_, "", 28)
    {
        label_.setFillColor(sf::Color::White);
        label_.setStyle(sf::Text::Bold);
        enterSystem(startingSystemIndex);
    }

    const char* name() const override
    {
        return "system";
    }

    World& world() override
    {
        return world_;
    }

    const World& world() const override
    {
        return world_;
    }

    /** Temporary stand-in for the warp scene: cycle systems directly with [ and ]. */
    void handleEvent(const sf::Event& event, const sf::RenderWindow&) override
    {
        if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>())
        {
            if (keyPressed->code == sf::Keyboard::Key::RBracket)
                enterSystem(currentSystemIndex_ + 1);

            if (keyPressed->code == sf::Keyboard::Key::LBracket)
                enterSystem(currentSystemIndex_ - 1);
        }
    }

    void drawOverlay(sf::RenderTarget& target) override
    {
        target.draw(label_);
    }

private:
    Galaxy& galaxy_;
    World world_;
    int currentSystemIndex_ = -1;
    sf::Font font_;
    sf::Text label_;

    /** Rebuilds the world from the system's seed. Same index always gives the same layout. */
    void enterSystem(int systemIndex)
    {
        const int systemCount = static_cast<int>(galaxy_.systems.size());
        systemIndex = std::clamp(systemIndex, 0, systemCount - 1);

        const SystemInfo& info = galaxy_.systems[static_cast<std::size_t>(systemIndex)];
        std::mt19937 rng(deriveSystemSeed(galaxy_.seed, systemIndex));

        world_ = World();

        world_.star = procgen::generateStar(rng);

        for (int planetIndex = 0; planetIndex < info.planetCount; ++planetIndex)
            world_.planets.push_back(procgen::generatePlanet(rng, planetIndex, world_.star));

        world_.stationActive = info.stationCount > 0 && !world_.planets.empty();

        if (world_.stationActive)
        {
            const procgen::StationPlacement placement = procgen::generateStationPlacement(rng, world_.planets);
            world_.station = placement.station;
            world_.stationHostPlanetIndex = placement.hostPlanetIndex;
            world_.stationOrbitRadius = placement.orbitRadius;
            world_.stationOrbitAngle = placement.orbitAngle;
            world_.stationOrbitSpeed = placement.orbitSpeed;
        }

        // Outer bound for NPC roaming, sized to comfortably contain every planet's orbit.
        float outerRadius = world_.star.radius * 3.f;

        for (const Planet& planet : world_.planets)
            outerRadius = std::max(outerRadius, length(planet.position) + planet.radius);

        world_.systemOuterRadius = outerRadius + 5000.f;

        world_.npcShips.assign(static_cast<std::size_t>(info.npcShipCount), NpcShip{});

        std::uniform_real_distribution<float> initialStaggerDist(0.f, npc_ai::minWarpOutDuration);

        for (NpcShip& npc : world_.npcShips)
        {
            ObjLoadOptions npcOptions;
            npcOptions.scale = 200.f;
            npcOptions.rotationDegrees = {0.f, -90.f, -90.f};
            npcOptions.centerOnOrigin = true;
            npc.ship.loadObjModel("assets/objects/ships/banshee.obj", npcOptions);

            npc.state = NpcState::Inactive;
            npc.stateTimer = 0.f;
            npc.wakeDelay = initialStaggerDist(world_.npcRng);
        }

        world_.playerShip.position = procgen::shipSpawnPosition(world_.star, world_.planets);
        world_.playerShip.velocity = {};
        world_.playerShip.yaw = 0.f;
        world_.playerShip.pitch = 0.f;
        world_.playerShip.throttle = 0.f;
        world_.playerShip.reverseThrust = false;

        ObjLoadOptions options_ship;
        options_ship.scale = 100.f;
        options_ship.rotationDegrees = {-90.f, 0.f, 0.f};
        options_ship.centerOnOrigin = true;
        world_.playerShip.loadObjModel("assets/objects/ships/banshee.obj", options_ship);

        ObjLoadOptions options_station_s;
        options_station_s.scale = 130.f;
        options_station_s.centerOnOrigin = true;
        if (!world_.station.loadObjModel("assets/objects/stations/station_s.obj", options_station_s))
            std::cerr << "[dusk] failed to load assets/objects/stations/station_s.obj "
                         "(is the assets folder next to the executable?)\n";

        currentSystemIndex_ = systemIndex;
        label_.setString(
            info.name + "  |  system " + std::to_string(systemIndex + 1) + "/" + std::to_string(systemCount) +
            "  |  [ / ] to travel"
        );
    }
};

#endif //DUSK_SYSTEM_SCENE_H