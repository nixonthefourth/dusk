//
// Created by Mykyta Khomiakov on 26/07/2026.
//
// The main game scene: one star system, rebuilt from its seed on entry. Owns the docking
// computer, the station menu, the galactic chart and system map, target locking, and the
// hyperspace jump sequence between systems.
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
#include "systems/docking_fees.h++"
#include "systems/save_game.h++"
#include "systems/trading.h++"
#include "systems/upgrades.h++"
#include "ui/station_menu.h++"
#include "ui/system_map.h++"
#include "ui/style.h++"
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
    /**
     * Starts a game from the main menu: a new commander in a fresh slot, or a loaded save. Either
     * way the commander begins docked at the station of the save's system (saves are made at
     * stations), and a new game is saved straight away so its slot shows as taken.
     */
    SystemScene(Galaxy& galaxy, const GameLaunch& launch)
        : SystemScene(galaxy, std::clamp(launch.save.systemIndex, 0, static_cast<int>(galaxy.systems.size()) - 1))
    {
        saveSlot_ = launch.slot;
        applySave(launch.save);

        if (launch.isNewGame && saveSlot_ >= 0)
            saveToSlot(false);
        else
            refreshSlotSummary();
    }

    /** Binds this scene to a galaxy and enters one of its systems, with no save slot (tests and tools). */
    SystemScene(Galaxy& galaxy, int startingSystemIndex)
        : galaxy_(galaxy),
          font_("assets/fonts/Jersey15-Regular.ttf"),
          label_(font_, "", 28),
          statusText_(font_, "", 24),
          hintText_(font_, "[G] GALAXY MAP\n[M] SYSTEM MAP\n[T] TARGET\n[C] DOCK", 15),
          messageText_(font_, "", 30)
    {
        statusText_.setFillColor(style::dockingStatus);
        hintText_.setFillColor(style::keyHints);
        hintText_.setLineSpacing(0.95f);
        hintText_.setPosition({4.f, 38.f});
        messageText_.setFillColor(style::message);
        messageText_.setStyle(sf::Text::Bold);

        label_.setFillColor(style::systemLabel);
        label_.setStyle(sf::Text::Bold);

        // The galaxy's markets and trader agents, with a little history already run.
        trade_.initialise(galaxy_);
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

    /** Read-only access to the current system's world. */
    const World& world() const override
    {
        return world_;
    }

    /**
     * Routes input by priority: nothing during the jump itself, then an open map, then the station
     * menu, then the flight keys (G, M, T, C, and while docked Enter/L).
     */
    void handleEvent(const sf::Event& event, const sf::RenderWindow& window) override
    {
        // Once the jump itself starts, the sequence plays out untouched.
        if (inHyperspaceSequence())
            return;

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

        if (hyperspace_.phase == HyperspacePhase::Countdown && keyPressed->code == sf::Keyboard::Key::Escape)
        {
            hyperspace_ = {};
            docking::showMessage(docking_, "HYPERSPACE ABORTED", 2.f);
            return;
        }

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
            if (hyperspace_.phase == HyperspacePhase::Countdown)
            {
                docking::showMessage(docking_, "HYPERSPACE COUNTDOWN IN PROGRESS", 2.f);
                return;
            }

            if (docking_.phase == DockingPhase::Idle)
            {
                // The docking computer flies to the station, so lock it as the target too.
                if (docking::engage(docking_, world_))
                {
                    world_.target.type = TargetType::Station;

                    char note[96];
                    std::snprintf(note, sizeof(note), commander_.hasDockingComputer ? "DOCKING COMPUTER ENGAGED  (FEE %.0f CR)" : "UNION TUG ENGAGED  (%.0f CR ON ARRIVAL)",
                                  dockingChargeFor(commander_));
                    docking::showMessage(docking_, note, 3.f);
                }
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

    /**
     * Escape closes an open map or the station screen, or aborts a hyperspace countdown, instead
     * of quitting the game. (The station screen was missing from this list, so Escape there quit
     * the game outright rather than closing the screen.)
     */
    bool capturesEscape() const override
    {
        return mapView_ != MapView::None || hyperspace_.phase != HyperspacePhase::None || stationMenuOpen_;
    }

    /** The flight HUD is hidden while a full-screen map or the station screen is up, and for the jump itself. */
    bool showsHud() const override
    {
        return mapView_ == MapView::None && !inHyperspaceSequence() && !stationMenuOpen_;
    }

    /** Hands main.cpp a requested transition (MAIN MENU from the station) exactly once. */
    SceneTransition consumeTransition() override
    {
        const SceneTransition transition = pendingTransition_;
        pendingTransition_ = SceneTransition::None;
        return transition;
    }

    /** Travel-animation state for the effects renderer. */
    TravelEffects travelEffects() const override
    {
        TravelEffects effects;
        effects.hyperspacePhase = hyperspace_.phase;
        effects.phaseTime = hyperspace_.time;
        effects.cruiseEngageBurst = cruiseEngageBurst_;
        effects.cruiseDropFlash = cruiseDropFlash_;

        const float duration = hyperspacePhaseDuration(hyperspace_.phase);

        if (hyperspace_.phase == HyperspacePhase::Countdown)
        {
            // During the countdown, "progress" is how far the final gathering has got.
            effects.phaseProgress = std::clamp((hyperspace_.time - (duration - countdownGatherTime)) / countdownGatherTime, 0.f, 1.f);
        }
        else if (duration > 0.f)
        {
            effects.phaseProgress = std::clamp(hyperspace_.time / duration, 0.f, 1.f);
        }

        return effects;
    }

    /** Hyperspace widens the view as the stars stretch, then lets it settle on arrival. */
    void updateCamera(Camera& camera, float dt, ShipCameraRig& rig) override
    {
        const float duration = hyperspacePhaseDuration(hyperspace_.phase);
        const float progress = duration > 0.f ? std::clamp(hyperspace_.time / duration, 0.f, 1.f) : 0.f;

        switch (hyperspace_.phase)
        {
            case HyperspacePhase::Countdown:
            {
                // A held breath in the last moments of the countdown.
                const float gather = std::clamp((hyperspace_.time - (duration - countdownGatherTime)) / countdownGatherTime, 0.f, 1.f);
                rig.sceneFovOffset = -5.f * gather;
                break;
            }

            case HyperspacePhase::Accelerate: rig.sceneFovOffset = -5.f + 40.f * progress * progress; break;
            case HyperspacePhase::Tunnel: rig.sceneFovOffset = 35.f; break;
            case HyperspacePhase::Arrive: rig.sceneFovOffset = 35.f * (1.f - progress) * (1.f - progress); break;
            case HyperspacePhase::None: rig.sceneFovOffset = 0.f; break;
        }

        Scene::updateCamera(camera, dt, rig);
    }

    /** Read-only views of the commander (credits, hold, fitted bay) and the trading economy, for tests and tools. */
    const Commander& commander() const
    {
        return commander_;
    }

    /** The trading economy: markets, prices and trader agents. */
    const TradeNetwork& tradeNetwork() const
    {
        return trade_;
    }

    /** Read-only view of the docking computer, e.g. for debugging or future HUD elements. */
    const DockingComputer& dockingComputer() const
    {
        return docking_;
    }

    /** True while the docked station menu is showing. */
    bool stationMenuOpen() const
    {
        return stationMenuOpen_;
    }

    /** The player flies the ship only while the docking computer is idle and no map has the keyboard. */
    bool acceptsShipInput() const override
    {
        return !docking::controlsShip(docking_) && mapView_ == MapView::None && !inHyperspaceSequence();
    }

    /**
     * One physics sub-step: the world (with the player under autopilot when docking), the docking
     * computer, then the cruise and hyperspace animation timers.
     */
    void updatePhysics(float dt) override
    {
        const bool autopilot = docking::controlsShip(docking_);
        const DockingPhase phaseBefore = docking_.phase;

        updateWorldPhysics(world_, dt, !autopilot);
        docking::update(docking_, world_, dt);

        if (phaseBefore != DockingPhase::Docked && docking_.phase == DockingPhase::Docked)
        {
            chargeDockingFee();
            openStationMenu();
        }

        // The galaxy's traders carry on while you play, and the ship feels what's in its hold.
        trade_.advance(trade_.time() + static_cast<double>(dt));
        syncShipLoad();

        updateCruiseTransitionEffects(dt);
        updateHyperspace(dt);
        playTime_ += dt;
    }

    /**
     * Screen-space drawing over the 3D view: a full-screen map if one is open, the hyperspace text
     * during a jump, otherwise the system name, key hints, docking status and station menu.
     */
    void drawOverlay(sf::RenderTarget& target) override
    {
        if (mapView_ == MapView::Galaxy)
        {
            galaxyMap_.draw(
                target,
                font_,
                galaxy_,
                currentSystemIndex_,
                jumpAvailable(),
                jumpBlockedReason(),
                jumpRangeLightYears(world_.playerShip),
                world_.playerShip.hyperspaceFuelPerLightYear,
                ChartIntel{commander_.hasPoliticalScanner, commander_.hasEconomicsScanner}
            );
            return;
        }

        if (mapView_ == MapView::System)
        {
            systemMap_.draw(target, font_, world_, currentSystemName_);
            return;
        }

        if (inHyperspaceSequence())
        {
            drawHyperspaceText(target);
            return;
        }

        target.draw(label_);
        target.draw(hintText_); // a short column under the system name, clear of the heading tape
        drawDockingStatus(target);

        if (hyperspace_.phase == HyperspacePhase::Countdown)
            drawHyperspaceText(target);

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

    DockingComputer docking_;
    bool stationMenuOpen_ = false;

    /** Which full-screen map, if any, is open over the flight view. */
    enum class MapView { None, Galaxy, System };
    MapView mapView_ = MapView::None;

    GalaxyMap galaxyMap_;
    SystemMap systemMap_;

    /** The hyperspace jump in progress, if any. */
    struct HyperspaceJump {
        HyperspacePhase phase = HyperspacePhase::None;
        float time = 0.f;
        int destination = -1;
        float distance = 0.f;
        bool arrived = false; // destination already swapped in
    };

    HyperspaceJump hyperspace_;

    /** Seconds each hyperspace phase lasts. The countdown is a nod to Elite's; shortened to keep it snappy. */
    static constexpr float countdownTime = 5.f;
    static constexpr float accelerateTime = 1.3f;
    static constexpr float tunnelTime = 2.8f;
    static constexpr float arriveTime = 1.1f;

    /** How long, at the end of the countdown, energy visibly gathers at the nose. */
    static constexpr float countdownGatherTime = 1.5f;

    /** Cruise animation timers, each running 1 to 0, and last step's cruise state for spotting changes. */
    float cruiseEngageBurst_ = 0.f;
    float cruiseDropFlash_ = 0.f;
    bool wasCruising_ = false;

    /** Highlighted station-menu option: 0 = STAY, 1 = LEAVE. */
    /** The docked station screen, and the commander whose credits it spends. */
    StationMenu stationMenu_;
    Commander commander_;

    /** Every system's market and the trader agents moving goods between them. */
    TradeNetwork trade_;

    /** The save slot this game belongs to (0-2), or -1 if it can't be saved; time played; what's in the slot now. */
    int saveSlot_ = -1;
    double playTime_ = 0.0;
    std::optional<SaveGame> slotSummary_;

    /** A transition requested from inside the game (MAIN MENU on the station screen). */
    SceneTransition pendingTransition_ = SceneTransition::None;


    /** Rebuilds the world from the system's seed. Same index always gives the same layout. */
    void enterSystem(int systemIndex)
    {
        const int systemCount = static_cast<int>(galaxy_.systems.size());
        systemIndex = std::clamp(systemIndex, 0, systemCount - 1);

        const SystemInfo& info = galaxy_.systems[static_cast<std::size_t>(systemIndex)];
        std::mt19937 rng(deriveSystemSeed(galaxy_.seed, systemIndex));

        // Fuel belongs to the ship, which is rebuilt with the world: carry it across the jump.
        const float carriedFuel = world_.playerShip.fuel;

        world_ = World();
        syncShipLoad(); // sets the tank size from the fitted module before the carried fuel is clamped to it
        world_.playerShip.fuel = std::min(carriedFuel, world_.playerShip.fuelCapacity);
        docking_ = DockingComputer();
        stationMenuOpen_ = false;
        wasCruising_ = false;
        cruiseEngageBurst_ = 0.f;
        cruiseDropFlash_ = 0.f;

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
            npc.ship.usesFuel = false; // NPCs never run dry

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

    /** Shows the station menu with STAY selected; called automatically when docking completes. */
    void openStationMenu()
    {
        stationMenuOpen_ = true;
        stationMenu_.open();
    }

    /** Gathers what the station screen shows: credits, fuel, mass, range, and the price quotes. */
    StationMenuView stationMenuView() const
    {
        const Ship& ship = world_.playerShip;
        const float price = fuelPricePerTonne(galaxy_.systems[static_cast<std::size_t>(currentSystemIndex_)]);

        StationMenuView view;
        view.title = currentSystemName_ + " STATION";
        view.commanderName = commander_.name;
        view.credits = commander_.credits;
        view.spendable = spendableCredits(commander_);
        view.reserve = spendingReserve(commander_);
        view.fuel = ship.fuel;
        view.fuelCapacity = ship.fuelCapacity;
        view.hullMass = ship.mass;
        view.totalMass = shipTotalMass(ship);
        view.jumpRangeLY = jumpRangeLightYears(ship);
        view.fullTankRangeLY = ship.hyperspaceFuelPerLightYear > 0.f ? ship.fuelCapacity / ship.hyperspaceFuelPerLightYear : 0.f;
        view.pricePerTonne = price;
        view.fillQuote = quoteRefuel(ship, commander_, price, ship.fuelCapacity);
        view.oneTonneQuote = quoteRefuel(ship, commander_, price, 1.f);

        // Hold, market and upgrades.
        view.cargoUsed = commander_.cargo.total();
        view.cargoCapacity = commander_.cargoCapacity();
        view.cargoModuleName = cargoModuleName(commander_.cargoModule);
        view.traderNote = trade_.describeTraderEvent(currentSystemIndex_);

        for (int good = 0; good < goodCount; ++good)
        {
            MarketRowView row;
            row.name = upperCase(goodName(good));
            row.buyPrice = trade_.buyPrice(currentSystemIndex_, good);
            row.sellPrice = trade_.sellPrice(currentSystemIndex_, good);
            row.stock = static_cast<int>(std::floor(trade_.stock(currentSystemIndex_, good)));
            row.held = commander_.cargo.of(good);
            row.versusAverage = trade_.versusAverage(currentSystemIndex_, good);
            row.canBuy = trade_.quoteBuy(currentSystemIndex_, good, 1, commander_.cargoRoom(), spendableCredits(commander_)).tonnes > 0;
            row.canSell = row.held > 0;
            view.market.push_back(row);
        }

        for (const ShipUpgrade& upgrade : upgradeCatalogue())
            view.upgrades.push_back({upgrade.name, upgrade.description, upgrade.price, offerFor(upgrade, commander_)});

        view.saveSlot = saveSlot_;
        view.hasSave = slotSummary_.has_value();

        if (slotSummary_)
        {
            view.savedAt = slotSummary_->savedAt;
            view.playTime = formatPlayTime(slotSummary_->playTimeSeconds);
            const int savedIndex = std::clamp(slotSummary_->systemIndex, 0, static_cast<int>(galaxy_.systems.size()) - 1);
            view.savedSystem = galaxy_.systems[static_cast<std::size_t>(savedIndex)].name;
        }

        return view;
    }

    /** Passes input to the station screen, then acts on it: buy fuel, launch, or close (staying docked). */
    void handleStationMenuEvent(const sf::Event& event, const sf::RenderWindow& window)
    {
        const float price = fuelPricePerTonne(galaxy_.systems[static_cast<std::size_t>(currentSystemIndex_)]);
        Ship& ship = world_.playerShip;

        const StationMenuAction action = stationMenu_.handleEvent(event, window);

        switch (action)
        {
            case StationMenuAction::Close:
                stationMenuOpen_ = false;
                break;

            case StationMenuAction::Launch:
                stationMenuOpen_ = false;
                docking::launch(docking_);
                break;

            case StationMenuAction::RefuelFull:
            case StationMenuAction::RefuelOneTonne:
            {
                // The menu says which button was used: FILL TANK buys the whole tank, BUY 1 t one
                // tonne, whether it was clicked or reached by keyboard. (This used to guess from
                // the input device, so clicking BUY 1 t, which isn't a key press, bought the tank.)
                const float wanted = action == StationMenuAction::RefuelFull ? ship.fuelCapacity : 1.f;
                const RefuelQuote bought = buyFuel(ship, commander_, price, wanted);

                char message[64];

                if (bought.tonnes > 0.f)
                    std::snprintf(message, sizeof(message), "REFUELLED %.1f t  -%.0f CR", bought.tonnes, bought.cost);
                else
                {
                    // Short of credits, or holding enough but not enough to keep the docking reserve?
                    if (bought.tankFull)
                        std::snprintf(message, sizeof(message), "TANK ALREADY FULL");
                    else if (commander_.credits >= price * 0.1f)
                        std::snprintf(message, sizeof(message), "KEEPING %.0f CR FOR DOCKING", spendingReserve(commander_));
                    else
                        std::snprintf(message, sizeof(message), "NOT ENOUGH CREDITS");
                }

                docking::showMessage(docking_, message, 2.f);
                break;
            }

            case StationMenuAction::BuyGoodOne:
            case StationMenuAction::BuyGoodMax:
            case StationMenuAction::SellGoodOne:
            case StationMenuAction::SellGoodAll:
                tradeSelectedGood(action);
                break;

            case StationMenuAction::InstallUpgrade:
                installSelectedUpgrade();
                break;

            case StationMenuAction::SaveGame:
                saveToSlot(true);
                break;

            case StationMenuAction::LoadGame:
                if (const auto save = saveSlot_ >= 0 ? readSave(saveSlot_) : std::nullopt)
                {
                    applySave(*save);
                    refreshSlotSummary();
                    docking::showMessage(docking_, "GAME LOADED", 2.f);
                }
                else
                {
                    docking::showMessage(docking_, "NOTHING SAVED IN THIS SLOT", 2.f);
                }
                break;

            case StationMenuAction::MainMenu:
                pendingTransition_ = SceneTransition::MainMenu;
                break;

            case StationMenuAction::None:
                break;
        }
    }

    /* ---- Trading ----------------------------------------------------------------------------- */

    /** Capitals, the way goods are shown on the station screen. */
    static std::string upperCase(std::string text)
    {
        for (char& c : text)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        return text;
    }

    /**
     * Takes the Space Union's docking fee when a docking completes (not when it's cancelled, and
     * not when a saved game loads straight into the station), and says what was taken.
     */
    void chargeDockingFee()
    {
        const bool computer = commander_.hasDockingComputer;
        const DockingCharge charge = chargeForDocking(commander_);
        char message[96];

        if (charge.paid + 0.5 >= charge.due)
            std::snprintf(message, sizeof(message), "%s  -%.0f CR", computer ? "SPACE UNION DOCKING FEE" : "UNION TUG FEE", charge.paid);
        else
            std::snprintf(message, sizeof(message), "%s  -%.0f CR (ALL YOU HAD)", computer ? "SPACE UNION DOCKING FEE" : "UNION TUG FEE", charge.paid);

        docking::showMessage(docking_, message, 4.f);
    }

    /** Copies the hold and the fitted tank onto the ship, so the thrusters, turn rate and HUD all feel them. */
    void syncShipLoad()
    {
        world_.playerShip.fuelCapacity = commander_.fuelCapacity();
        world_.playerShip.cargoMass = static_cast<float>(commander_.cargo.total());
        world_.playerShip.cargoCapacity = static_cast<float>(commander_.cargoCapacity());
    }

    /**
     * Buys or sells the highlighted good at this system's market: one tonne or as many as hold,
     * stock and credits allow when buying, one tonne or the lot when selling. The market works out
     * the price tonne by tonne; this moves the goods and the credits and says what happened.
     */
    void tradeSelectedGood(StationMenuAction action)
    {
        const int system = currentSystemIndex_;
        const int good = stationMenu_.selectedGood();
        const std::string name = upperCase(goodName(good));
        char message[96];

        if (action == StationMenuAction::BuyGoodOne || action == StationMenuAction::BuyGoodMax)
        {
            const int wanted = action == StationMenuAction::BuyGoodMax ? std::max(1, commander_.cargoRoom()) : 1;
            const trading::TradeQuote quote = trade_.buy(system, good, wanted, commander_.cargoRoom(), spendableCredits(commander_));

            if (quote.tonnes > 0)
            {
                commander_.credits -= quote.value;
                commander_.cargo.add(good, quote.tonnes);
                std::snprintf(message, sizeof(message), "BOUGHT %d t %s  -%.0f CR", quote.tonnes, name.c_str(), quote.value);
            }
            else
            {
                if (quote.limit == trading::Limit::SoldOut)
                    std::snprintf(message, sizeof(message), "SOLD OUT");
                else if (quote.limit == trading::Limit::HoldFull)
                    std::snprintf(message, sizeof(message), "HOLD FULL");
                else if (reserveBlocks(commander_, static_cast<double>(trade_.buyPrice(system, good))))
                    std::snprintf(message, sizeof(message), "KEEPING %.0f CR FOR DOCKING", spendingReserve(commander_));
                else
                    std::snprintf(message, sizeof(message), "NOT ENOUGH CREDITS");
            }
        }
        else
        {
            const int held = commander_.cargo.of(good);
            const int wanted = action == StationMenuAction::SellGoodAll ? std::max(1, held) : 1;
            const trading::TradeQuote quote = trade_.sell(system, good, wanted, held);

            if (quote.tonnes > 0)
            {
                commander_.credits += quote.value;
                commander_.cargo.add(good, -quote.tonnes);
                std::snprintf(message, sizeof(message), "SOLD %d t %s  +%.0f CR", quote.tonnes, name.c_str(), quote.value);
            }
            else
            {
                std::snprintf(message, sizeof(message), "NOTHING TO SELL");
            }
        }

        syncShipLoad();
        docking::showMessage(docking_, message, 2.f);
    }

    /** Buys the highlighted upgrade if allowed, and says what happened. */
    void installSelectedUpgrade()
    {
        const std::size_t index = static_cast<std::size_t>(std::max(0, stationMenu_.selectedUpgrade()));
        const UpgradeOffer offer = buyUpgrade(commander_, index);
        char message[96];

        switch (offer.status)
        {
            case UpgradeStatus::Available:
                std::snprintf(message, sizeof(message), "%s INSTALLED  -%.0f CR", upgradeCatalogue()[index].name, offer.cost);
                break;

            case UpgradeStatus::Installed: std::snprintf(message, sizeof(message), "ALREADY INSTALLED"); break;
            case UpgradeStatus::HaveBetter: std::snprintf(message, sizeof(message), "YOU ALREADY HAVE A BETTER ONE"); break;
            case UpgradeStatus::CantAfford:
                if (offer.reserveBlocked)
                    std::snprintf(message, sizeof(message), "KEEPING %.0f CR IN RESERVE", offer.reserve);
                else
                    std::snprintf(message, sizeof(message), "NOT ENOUGH CREDITS");
                break;
        }

        syncShipLoad();
        docking::showMessage(docking_, message, 2.5f);
    }

    /* ---- Saving and loading ------------------------------------------------------------------ */

    /** The current game as a save: commander, credits, fuel, this system and time played. */
    SaveGame currentSave() const
    {
        SaveGame save;
        save.commanderName = commander_.name;
        save.credits = commander_.credits;
        save.galaxySeed = galaxy_.seed;
        save.systemIndex = currentSystemIndex_;
        save.systemName = currentSystemName_;
        save.fuel = world_.playerShip.fuel;

        save.cargoModule = static_cast<int>(commander_.cargoModule);
        save.fuelTank = static_cast<int>(commander_.fuelTank);
        save.autoDock = commander_.hasDockingComputer;
        save.economicsScanner = commander_.hasEconomicsScanner;
        save.politicalScanner = commander_.hasPoliticalScanner;

        for (int good = 0; good < goodCount; ++good)
        {
            if (commander_.cargo.of(good) > 0)
                save.cargo.emplace_back(goodName(good), commander_.cargo.of(good));
        }

        trade_.fillSave(save);
        save.playTimeSeconds = playTime_;
        return save;
    }

    /** Writes the current game to this commander's slot; with `announce`, says so (or why it failed). */
    void saveToSlot(bool announce)
    {
        std::string error;
        const bool saved = saveSlot_ >= 0 && writeSave(saveSlot_, currentSave(), &error);
        refreshSlotSummary();

        if (announce)
            docking::showMessage(docking_, saved ? "GAME SAVED" : "SAVE FAILED: " + (error.empty() ? std::string("NO SLOT") : error), 2.5f);
    }

    /** Re-reads what's in the slot, for the SAVE GAME page. */
    void refreshSlotSummary()
    {
        slotSummary_ = saveSlot_ >= 0 ? readSave(saveSlot_) : std::nullopt;
    }

    /**
     * Puts the game into a save's state: the commander and their credits, the saved system (rebuilt
     * from its seed), the fuel aboard, and the ship parked in that system's station with the station
     * screen open. If the system has no station (only possible for a brand-new game), the ship
     * starts in space at the usual spawn point instead.
     */
    void applySave(const SaveGame& save)
    {
        commander_.name = save.commanderName;
        commander_.credits = save.credits;

        commander_.cargoModule = cargoModuleFromInt(save.cargoModule);
        commander_.fuelTank = fuelTankFromInt(save.fuelTank);
        commander_.hasDockingComputer = save.autoDock;
        commander_.hasEconomicsScanner = save.economicsScanner;
        commander_.hasPoliticalScanner = save.politicalScanner;
        commander_.cargo = {};

        for (const auto& [name, tonnes] : save.cargo)
            commander_.cargo.add(goodIndex(name), tonnes);

        commander_.cargo.trimTo(commander_.cargoCapacity());

        // Saves from before trading have no network; the one built at start-up stands in for it.
        if (save.hasTrade)
            trade_.restore(galaxy_, save);
        playTime_ = save.playTimeSeconds;
        hyperspace_ = {};
        mapView_ = MapView::None;

        const int systemIndex = std::clamp(save.systemIndex, 0, static_cast<int>(galaxy_.systems.size()) - 1);
        enterSystem(systemIndex);
        world_.playerShip.fuel = std::clamp(save.fuel, 0.f, world_.playerShip.fuelCapacity);

        if (docking::dockImmediately(docking_, world_))
            openStationMenu();
    }

    /* ---- Travel animations ------------------------------------------------------------------- */

    static float hyperspacePhaseDuration(HyperspacePhase phase)
    {
        switch (phase)
        {
            case HyperspacePhase::Countdown: return countdownTime;
            case HyperspacePhase::Accelerate: return accelerateTime;
            case HyperspacePhase::Tunnel: return tunnelTime;
            case HyperspacePhase::Arrive: return arriveTime;
            case HyperspacePhase::None: break;
        }

        return 0.f;
    }

    /** True from the moment the stars start stretching until the new system has settled. */
    bool inHyperspaceSequence() const
    {
        return hyperspace_.phase == HyperspacePhase::Accelerate ||
               hyperspace_.phase == HyperspacePhase::Tunnel ||
               hyperspace_.phase == HyperspacePhase::Arrive;
    }

    /** Fires the burst when cruise engages and the flash when it drops out, then lets both fade. */
    void updateCruiseTransitionEffects(float dt)
    {
        const bool cruising = world_.playerShip.cruiseEngaged;

        if (cruising && !wasCruising_)
            cruiseEngageBurst_ = 1.f;
        else if (!cruising && wasCruising_)
            cruiseDropFlash_ = 1.f;

        wasCruising_ = cruising;
        cruiseEngageBurst_ = std::max(0.f, cruiseEngageBurst_ - dt / 0.6f);
        cruiseDropFlash_ = std::max(0.f, cruiseDropFlash_ - dt / 0.4f);
    }

    /** Steps the hyperspace sequence through its phases, swapping the destination in mid-tunnel. */
    void updateHyperspace(float dt)
    {
        if (hyperspace_.phase == HyperspacePhase::None)
            return;

        hyperspace_.time += dt;

        // Swap systems while the tunnel hides everything (40% of the way through).
        if (hyperspace_.phase == HyperspacePhase::Tunnel && !hyperspace_.arrived && hyperspace_.time >= tunnelTime * 0.4f)
        {
            const HyperspaceJump jump = hyperspace_;

            // Pay for the jump before the ship is carried into the new system.
            Ship& ship = world_.playerShip;
            ship.fuel = std::max(0.f, ship.fuel - jumpFuelCost(ship, jump.distance));

            // The trip takes galactic time: the traders keep moving while you travel.
            trade_.advance(trade_.time() + static_cast<double>(jump.distance) * trading::jumpSecondsPerLightYear);

            enterSystem(jump.destination);
            hyperspace_ = jump;
            hyperspace_.arrived = true;
        }

        if (hyperspace_.time < hyperspacePhaseDuration(hyperspace_.phase))
            return;

        hyperspace_.time = 0.f;

        switch (hyperspace_.phase)
        {
            case HyperspacePhase::Countdown:
                // Cruising during the countdown burns fuel: if the jump can no longer be paid for,
                // it is called off here rather than stranding the ship mid-jump.
                if (world_.playerShip.fuel + 1e-4f < jumpFuelCost(world_.playerShip, hyperspace_.distance))
                {
                    hyperspace_ = {};
                    docking::showMessage(docking_, "HYPERSPACE ABORTED: NOT ENOUGH FUEL", 2.5f);
                    return;
                }

                // The jump takes over the screen: any open map gives way to it.
                hyperspace_.phase = HyperspacePhase::Accelerate;
                mapView_ = MapView::None;
                cancelCruiseCharge(world_.playerShip);
                break;
            case HyperspacePhase::Accelerate: hyperspace_.phase = HyperspacePhase::Tunnel; break;

            case HyperspacePhase::Tunnel:
            {
                hyperspace_.phase = HyperspacePhase::Arrive;

                char message[96];
                std::snprintf(message, sizeof(message), "ARRIVED IN %s  (%.1f LY)", currentSystemName_.c_str(), hyperspace_.distance);
                docking::showMessage(docking_, message, 3.5f);
                break;
            }

            case HyperspacePhase::Arrive:
            case HyperspacePhase::None:
                hyperspace_ = {};
                break;
        }
    }

    /** Centred text with its top at `y`. */
    void drawCentredText(sf::RenderTarget& target, const std::string& string, float y, unsigned size, sf::Color color)
    {
        sf::Text text(font_, string, size);
        text.setFillColor(color);
        const sf::FloatRect bounds = text.getLocalBounds();
        text.setOrigin({bounds.position.x + bounds.size.x * 0.5f, bounds.position.y});
        text.setPosition({static_cast<float>(target.getSize().x) * 0.5f, y});
        target.draw(text);
    }

    /** Elite-style countdown, then the destination readout inside the tunnel. */
    void drawHyperspaceText(sf::RenderTarget& target)
    {
        const float height = static_cast<float>(target.getSize().y);
        const std::string& destination = galaxy_.systems[static_cast<std::size_t>(std::max(0, hyperspace_.destination))].name;

        char distance[32];
        std::snprintf(distance, sizeof(distance), "%.1f LY", hyperspace_.distance);

        if (hyperspace_.phase == HyperspacePhase::Countdown)
        {
            const int secondsLeft = std::max(1, static_cast<int>(std::ceil(countdownTime - hyperspace_.time)));
            drawCentredText(target, "HYPERSPACE", height * 0.16f, 30, style::countdownTitle);
            drawCentredText(target, std::to_string(secondsLeft), height * 0.16f + 34.f, 64, style::countdownNumber);
            drawCentredText(target, destination + "   " + distance + "      [ESC] ABORT", height * 0.16f + 112.f, 18, style::countdownDetail);
            return;
        }

        if (hyperspace_.phase == HyperspacePhase::Tunnel)
        {
            drawCentredText(target, "HYPERSPACE", height - 74.f, 26, style::tunnelTitle);
            drawCentredText(target, destination + "   " + distance, height - 42.f, 18, style::tunnelDetail);
        }
    }

    /* ---- Maps -------------------------------------------------------------------------------- */

    /** Jumps are only possible in free flight: not docked, not under the docking computer. */
    bool jumpAvailable() const
    {
        return docking_.phase == DockingPhase::Idle && hyperspace_.phase == HyperspacePhase::None;
    }

    /** Text shown on the chart's jump button when a jump isn't possible right now. */
    std::string jumpBlockedReason() const
    {
        if (hyperspace_.phase != HyperspacePhase::None)
            return "JUMP IN PROGRESS";

        return docking_.phase == DockingPhase::Docked ? "LAUNCH FIRST" : "DOCKING IN PROGRESS";
    }

    /** Opens the galactic chart centred on the current system. */
    void openGalaxyMap()
    {
        galaxyMap_.open(galaxy_, currentSystemIndex_);
        mapView_ = MapView::Galaxy;
    }

    /** Opens the system map with the first planet highlighted. */
    void openSystemMap()
    {
        systemMap_.open();
        mapView_ = MapView::System;
    }

    /** Passes input to the chart, then acts on what it returns: close it, or start a jump countdown. */
    void handleGalaxyMapEvent(const sf::Event& event, const sf::RenderWindow& window)
    {
        switch (galaxyMap_.handleEvent(event, window, galaxy_, currentSystemIndex_, jumpAvailable(), jumpRangeLightYears(world_.playerShip)))
        {
            case GalaxyMapAction::Close:
                mapView_ = MapView::None;
                break;

            case GalaxyMapAction::Jump:
            {
                // Start the countdown; the sequence swaps the destination in mid-tunnel.
                hyperspace_ = {};
                hyperspace_.phase = HyperspacePhase::Countdown;
                hyperspace_.destination = galaxyMap_.selected();
                hyperspace_.distance = galacticDistance(
                    galaxy_.systems[static_cast<std::size_t>(currentSystemIndex_)],
                    galaxy_.systems[static_cast<std::size_t>(hyperspace_.destination)]
                );
                mapView_ = MapView::None;
                break;
            }

            case GalaxyMapAction::None:
                break;
        }
    }

    /**
     * Passes input to the system map, then acts on what it returns: close, switch to the chart, or
     * toggle the station target lock.
     */
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

    /** The docking computer's status (bottom right, above the dashboard) and any fading centred message. */
    void drawDockingStatus(sf::RenderTarget& target)
    {
        const sf::Vector2u size = target.getSize();
        std::string status = docking::phaseLabel(docking_.phase);

        if (docking_.phase == DockingPhase::Idle && world_.stationActive)
        {
            // The prompt says what docking will cost: the computer's fee, or the Union tug's.
            char prompt[64];
            std::snprintf(prompt, sizeof(prompt), commander_.hasDockingComputer ? "[C] DOCKING COMPUTER  %.0f CR" : "[C] UNION TUG  %.0f CR", dockingChargeFor(commander_));
            status = prompt;
        }
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
            messageText_.setFillColor(style::withAlpha(style::message, static_cast<int>(alpha)));
            ui::centerText(messageText_, {static_cast<float>(size.x) * 0.5f, static_cast<float>(size.y) * 0.22f});
            target.draw(messageText_);
        }
    }

    /** Veils the view and draws the station name over the STAY and LEAVE buttons. */
    void drawStationMenu(sf::RenderTarget& target)
    {
        stationMenu_.draw(target, font_, stationMenuView());

        // Purchase confirmations show over the station screen.
        if (docking_.messageTimer > 0.f)
        {
            const sf::Vector2u size = target.getSize();
            messageText_.setString(docking_.message);
            messageText_.setCharacterSize(20);
            const std::uint8_t alpha = static_cast<std::uint8_t>(255.f * std::min(1.f, docking_.messageTimer));
            messageText_.setFillColor(style::withAlpha(style::accent, static_cast<int>(alpha)));
            ui::centerText(messageText_, {static_cast<float>(size.x) * 0.5f, static_cast<float>(size.y) - 70.f});
            target.draw(messageText_);
            messageText_.setCharacterSize(30);
        }
    }
};

#endif //DUSK_SYSTEM_SCENE_H