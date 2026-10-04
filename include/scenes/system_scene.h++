//
// Created by Mykyta Khomiakov on 26/07/2026.
//

#ifndef DUSK_SYSTEM_SCENE_H
#define DUSK_SYSTEM_SCENE_H

#include "io/obj_loader.h++"
#include "procgen/galaxy.h++"
#include "procgen/planet_generation.h++"
#include "rendering/hud_renderer.h++"
#include "scenes/scene.h++"
#include "systems/docking_computer.h++"
#include "ui/galaxy_map.h++"
#include "ui/menu_button.h++"
#include "ui/system_map.h++"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
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
          label_(font_, "", 28),
          statusText_(font_, "", 24),
          hintText_(font_, "[G] GALAXY MAP\n[M] SYSTEM MAP\n[T] TARGET\n[C] DOCK", 15),
          messageText_(font_, "", 30),
          menuTitle_(font_, "", 40),
          stayText_(font_, "STAY", 42),
          leaveText_(font_, "LEAVE", 42)
    {
        statusText_.setFillColor(sf::Color(110, 220, 255));
        hintText_.setFillColor(sf::Color(90, 100, 110));
        hintText_.setLineSpacing(0.95f);
        hintText_.setPosition({4.f, 38.f});
        messageText_.setFillColor(sf::Color::White);
        messageText_.setStyle(sf::Text::Bold);
        menuTitle_.setFillColor(sf::Color::White);
        menuTitle_.setStyle(sf::Text::Bold);
        stayText_.setStyle(sf::Text::Bold);
        leaveText_.setStyle(sf::Text::Bold);

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

    void handleEvent(const sf::Event& event, const sf::RenderWindow& window) override
    {
        if (mapView_ == MapView::Galaxy)
        {
            handleGalaxyMapEvent(event, window);
            return;
        }

        if (mapView_ == MapView::System)
        {
            handleSystemMapEvent(event, window);
            return;
        }

        if (stationMenuOpen_)
        {
            handleStationMenuEvent(event, window);
            return;
        }

        const auto* keyPressed = event.getIf<sf::Event::KeyPressed>();

        if (!keyPressed)
            return;

        switch (keyPressed->code)
        {
            case sf::Keyboard::Key::G:
                openGalaxyMap();
                return;

            case sf::Keyboard::Key::M:
                openSystemMap();
                return;

            case sf::Keyboard::Key::T:
                cycleTarget(world_);
                return;

            default:
                break;
        }

        // C toggles the docking computer (Elite's docking-computer key).
        if (keyPressed->code == sf::Keyboard::Key::C)
        {
            if (docking_.phase == DockingPhase::Idle)
            {
                // The docking computer flies to the station, so lock it as the target too.
                if (docking::engage(docking_, world_))
                    world_.target.type = TargetType::Station;
            }
            else
            {
                docking::cancel(docking_);
            }

            return;
        }

        if (docking_.phase == DockingPhase::Docked)
        {
            if (keyPressed->code == sf::Keyboard::Key::Enter)
                openStationMenu();

            if (keyPressed->code == sf::Keyboard::Key::L)
                docking::launch(docking_);
        }
    }

    /** Escape closes an open map instead of quitting the game. */
    bool capturesEscape() const override
    {
        return mapView_ != MapView::None;
    }

    /** The flight HUD is hidden while a full-screen map is up. */
    bool showsHud() const override
    {
        return mapView_ == MapView::None;
    }

    /** Read-only view of the docking computer, e.g. for debugging or future HUD elements. */
    const DockingComputer& dockingComputer() const
    {
        return docking_;
    }

    bool stationMenuOpen() const
    {
        return stationMenuOpen_;
    }

    /** The player flies the ship only while the docking computer is idle and no map has the keyboard. */
    bool acceptsShipInput() const override
    {
        return !docking::controlsShip(docking_) && mapView_ == MapView::None;
    }

    void updatePhysics(float dt) override
    {
        const bool autopilot = docking::controlsShip(docking_);
        const DockingPhase phaseBefore = docking_.phase;

        updateWorldPhysics(world_, dt, !autopilot);
        docking::update(docking_, world_, dt);

        if (phaseBefore != DockingPhase::Docked && docking_.phase == DockingPhase::Docked)
            openStationMenu();
    }

    void drawOverlay(sf::RenderTarget& target) override
    {
        if (mapView_ == MapView::Galaxy)
        {
            galaxyMap_.draw(target, font_, galaxy_, currentSystemIndex_, jumpAvailable(), jumpBlockedReason());
            return;
        }

        if (mapView_ == MapView::System)
        {
            systemMap_.draw(target, font_, world_, currentSystemName_);
            return;
        }

        target.draw(label_);
        target.draw(hintText_); // a short column under the system name, clear of the heading tape
        drawDockingStatus(target);

        if (stationMenuOpen_)
            drawStationMenu(target);
    }

private:
    Galaxy& galaxy_;
    World world_;
    int currentSystemIndex_ = -1;
    std::string currentSystemName_;
    sf::Font font_;
    sf::Text label_;
    sf::Text statusText_;
    sf::Text hintText_;
    sf::Text messageText_;
    sf::Text menuTitle_;
    sf::Text stayText_;
    sf::Text leaveText_;

    DockingComputer docking_;
    bool stationMenuOpen_ = false;

    /** Which full-screen map, if any, is open over the flight view. */
    enum class MapView { None, Galaxy, System };
    MapView mapView_ = MapView::None;

    GalaxyMap galaxyMap_;
    SystemMap systemMap_;

    /** Highlighted station-menu option: 0 = STAY, 1 = LEAVE. */
    int menuSelection_ = 0;

    static constexpr int stayOption = 0;
    static constexpr int leaveOption = 1;

    /** Rebuilds the world from the system's seed. Same index always gives the same layout. */
    void enterSystem(int systemIndex)
    {
        const int systemCount = static_cast<int>(galaxy_.systems.size());
        systemIndex = std::clamp(systemIndex, 0, systemCount - 1);

        const SystemInfo& info = galaxy_.systems[static_cast<std::size_t>(systemIndex)];
        std::mt19937 rng(deriveSystemSeed(galaxy_.seed, systemIndex));

        world_ = World();
        docking_ = DockingComputer();
        stationMenuOpen_ = false;

        world_.star = procgen::generateStar(rng);

        for (int planetIndex = 0; planetIndex < info.planetCount; ++planetIndex)
            world_.planets.push_back(procgen::generatePlanet(rng, planetIndex, world_.star));

        // Belts come from their own RNG stream (keyed off the same system seed), so adding them
        // left every planet, station and NPC exactly where it was.
        world_.asteroidBelts = procgen::generateAsteroidBelts(
            deriveSystemSeed(galaxy_.seed, systemIndex),
            info.beltCount,
            world_.star,
            world_.planets
        );

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

        // Some planets carry a little debris belt of their own (never the station's host). Like
        // the star's belts, these use their own RNG stream, so nothing else in the system moves.
        {
            const std::uint32_t systemSeed = deriveSystemSeed(galaxy_.seed, systemIndex);
            std::vector<AsteroidBelt> planetBelts = procgen::generatePlanetBelts(
                systemSeed,
                world_.star,
                world_.planets,
                world_.asteroidBelts,
                world_.stationActive ? world_.stationHostPlanetIndex : -1
            );

            for (AsteroidBelt& belt : planetBelts)
                world_.asteroidBelts.push_back(std::move(belt));

            // Lone rocks drifting through the system share one small set of shapes.
            world_.looseRockShapes = procgen::generateLooseRockShapes(systemSeed, 6, 3);
            world_.looseCoarseShapeCount = 6;
            world_.driftRng.seed(systemSeed ^ 0x0D1F7u);
        }

        // Outer bound for NPC roaming, sized to comfortably contain every planet's orbit.
        float outerRadius = world_.star.radius * 3.f;

        for (const Planet& planet : world_.planets)
            outerRadius = std::max(outerRadius, length(planet.position) + planet.radius);

        world_.systemOuterRadius = outerRadius + 60000.f;

        world_.npcShips.assign(static_cast<std::size_t>(info.npcShipCount), NpcShip{});

        std::uniform_real_distribution<float> initialStaggerDist(0.f, npc_ai::minWarpOutDuration);

        for (NpcShip& npc : world_.npcShips)
        {
            ObjLoadOptions npcOptions;
            npcOptions.scale = 100.f; // same scale as the player's banshee, so ship-to-world proportions agree
            npcOptions.rotationDegrees = {0.f, -90.f, -90.f};
            npcOptions.centerOnOrigin = true;
            npc.ship.loadObjModel("assets/objects/ships/banshee.obj", npcOptions);

            npc.state = NpcState::Inactive;
            npc.stateTimer = 0.f;
            npc.wakeDelay = initialStaggerDist(world_.npcRng);
        }

        const procgen::SpawnPose spawn = procgen::shipSpawnPose(
            world_.star,
            world_.planets,
            world_.stationActive,
            world_.station.position,
            world_.stationHostPlanetIndex
        );

        world_.playerShip.position = spawn.position;
        world_.playerShip.previousPosition = spawn.position;
        world_.playerShip.velocity = {};
        world_.playerShip.yaw = std::atan2(spawn.facing.x, spawn.facing.z);
        world_.playerShip.pitch = std::asin(std::clamp(spawn.facing.y, -1.f, 1.f));
        clampShipPitch(world_.playerShip);
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
        {
            std::cerr << "[dusk] failed to load assets/objects/stations/station_s.obj "
                         "(is the assets folder next to the executable?)\n";
        }
        else
        {
            // station_s.obj: vertices 1-16 (0-based 0-15) outline the docking slot. The even ring sits
            // on the hull, the odd ring is the same outline two model units deeper, on the slot's back wall.
            const bool portOk = configureDockingPort(
                world_.station,
                {0, 1, 4, 6, 8, 10, 12, 14},
                {3, 2, 5, 7, 9, 11, 13, 15}
            );

            if (!portOk)
                std::cerr << "[dusk] station_s.obj docking slot vertices are out of range\n";

            world_.station.dockFacing = stationOrbitTangent(world_);
            refreshStationOrientation(world_.station);
        }

        currentSystemIndex_ = systemIndex;
        currentSystemName_ = info.name;
        label_.setString(info.name);
    }

    void openStationMenu()
    {
        stationMenuOpen_ = true;
        menuSelection_ = stayOption;
    }

    /** Applies the highlighted station-menu option. */
    void confirmStationMenu()
    {
        stationMenuOpen_ = false;

        if (menuSelection_ == leaveOption)
            docking::launch(docking_);
    }

    static sf::FloatRect menuButtonBounds(sf::Vector2u targetSize, int option)
    {
        return ui::stackedButtonBounds(targetSize, option, 2, 30.f);
    }

    void handleStationMenuEvent(const sf::Event& event, const sf::RenderWindow& window)
    {
        if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>())
        {
            switch (keyPressed->code)
            {
                case sf::Keyboard::Key::W:
                case sf::Keyboard::Key::S:
                case sf::Keyboard::Key::Left:
                case sf::Keyboard::Key::Right:
                case sf::Keyboard::Key::Tab:
                    menuSelection_ = menuSelection_ == stayOption ? leaveOption : stayOption;
                    break;

                case sf::Keyboard::Key::Num1:
                    menuSelection_ = stayOption;
                    confirmStationMenu();
                    break;

                case sf::Keyboard::Key::Num2:
                case sf::Keyboard::Key::L:
                    menuSelection_ = leaveOption;
                    confirmStationMenu();
                    break;

                case sf::Keyboard::Key::Enter:
                case sf::Keyboard::Key::Space:
                    confirmStationMenu();
                    break;

                default:
                    break;
            }
        }

        if (const auto* mouseMoved = event.getIf<sf::Event::MouseMoved>())
        {
            const sf::Vector2f mouse = ui::toVector2f(mouseMoved->position);

            for (int option : {stayOption, leaveOption})
            {
                if (menuButtonBounds(window.getSize(), option).contains(mouse))
                    menuSelection_ = option;
            }
        }

        if (const auto* mousePressed = event.getIf<sf::Event::MouseButtonPressed>())
        {
            if (mousePressed->button != sf::Mouse::Button::Left)
                return;

            const sf::Vector2f mouse = ui::toVector2f(mousePressed->position);

            for (int option : {stayOption, leaveOption})
            {
                if (menuButtonBounds(window.getSize(), option).contains(mouse))
                {
                    menuSelection_ = option;
                    confirmStationMenu();
                    return;
                }
            }
        }
    }

    /* ---- Maps -------------------------------------------------------------------------------- */

    /** Jumps are only possible in free flight: not docked, not under the docking computer. */
    bool jumpAvailable() const
    {
        return docking_.phase == DockingPhase::Idle;
    }

    std::string jumpBlockedReason() const
    {
        return docking_.phase == DockingPhase::Docked ? "LAUNCH FIRST" : "DOCKING IN PROGRESS";
    }

    void openGalaxyMap()
    {
        galaxyMap_.open(galaxy_, currentSystemIndex_);
        mapView_ = MapView::Galaxy;
    }

    void openSystemMap()
    {
        systemMap_.open();
        mapView_ = MapView::System;
    }

    void handleGalaxyMapEvent(const sf::Event& event, const sf::RenderWindow& window)
    {
        switch (galaxyMap_.handleEvent(event, window, galaxy_, currentSystemIndex_, jumpAvailable()))
        {
            case GalaxyMapAction::Close:
                mapView_ = MapView::None;
                break;

            case GalaxyMapAction::Jump:
            {
                // Stand-in for the warp scene: regenerate the destination from its seed, instantly.
                const int destination = galaxyMap_.selected();
                const float distance = galacticDistance(
                    galaxy_.systems[static_cast<std::size_t>(currentSystemIndex_)],
                    galaxy_.systems[static_cast<std::size_t>(destination)]
                );

                enterSystem(destination);
                mapView_ = MapView::None;

                char message[96];
                std::snprintf(message, sizeof(message), "ARRIVED IN %s  (%.1f LY)", currentSystemName_.c_str(), distance);
                docking::showMessage(docking_, message, 3.f);
                break;
            }

            case GalaxyMapAction::None:
                break;
        }
    }

    void handleSystemMapEvent(const sf::Event& event, const sf::RenderWindow& window)
    {
        switch (systemMap_.handleEvent(event, window, world_))
        {
            case SystemMapAction::Close:
                mapView_ = MapView::None;
                break;

            case SystemMapAction::OpenGalaxyMap:
                openGalaxyMap();
                break;

            case SystemMapAction::TargetStation:
                if (world_.stationActive)
                    world_.target.type = world_.target.type == TargetType::Station ? TargetType::None : TargetType::Station;
                break;

            case SystemMapAction::None:
                break;
        }
    }

    void drawDockingStatus(sf::RenderTarget& target)
    {
        const sf::Vector2u size = target.getSize();
        std::string status = docking::phaseLabel(docking_.phase);

        if (docking_.phase == DockingPhase::Idle && world_.stationActive)
            status = "[C] DOCKING COMPUTER";
        else if (docking::canCancel(docking_))
            status += "   [C] CANCEL";
        else if (docking_.phase == DockingPhase::Docked && !stationMenuOpen_)
            status += "   [ENTER] STATION MENU   [L] LAUNCH";

        if (!status.empty())
        {
            statusText_.setString(status);
            const sf::FloatRect bounds = statusText_.getLocalBounds();
            statusText_.setOrigin({bounds.position.x + bounds.size.x, bounds.position.y + bounds.size.y});
            statusText_.setPosition({static_cast<float>(size.x) - 18.f, static_cast<float>(size.y) - HudRenderer::dashboardHeight - 10.f});
            target.draw(statusText_);
        }

        if (docking_.messageTimer > 0.f && !stationMenuOpen_)
        {
            messageText_.setString(docking_.message);
            const std::uint8_t alpha = static_cast<std::uint8_t>(255.f * std::min(1.f, docking_.messageTimer));
            messageText_.setFillColor(sf::Color(255, 255, 255, alpha));
            ui::centerText(messageText_, {static_cast<float>(size.x) * 0.5f, static_cast<float>(size.y) * 0.22f});
            target.draw(messageText_);
        }
    }

    void drawStationMenu(sf::RenderTarget& target)
    {
        const sf::Vector2u size = target.getSize();

        sf::RectangleShape veil({static_cast<float>(size.x), static_cast<float>(size.y)});
        veil.setFillColor(sf::Color(0, 0, 0, 150));
        target.draw(veil);

        menuTitle_.setString(currentSystemName_ + " STATION");
        const sf::FloatRect stayBounds = menuButtonBounds(size, stayOption);
        ui::centerText(menuTitle_, {static_cast<float>(size.x) * 0.5f, stayBounds.position.y - 50.f});
        target.draw(menuTitle_);

        ui::drawButton(target, stayBounds, stayText_, menuSelection_ == stayOption, menuSelection_ == stayOption);
        ui::drawButton(
            target,
            menuButtonBounds(size, leaveOption),
            leaveText_,
            menuSelection_ == leaveOption,
            menuSelection_ == leaveOption
        );
    }
};

#endif //DUSK_SYSTEM_SCENE_H