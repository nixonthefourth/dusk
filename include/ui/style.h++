//
// Dusk's stylesheet: every colour the game draws with, in one place.
//
// Nothing else in the codebase writes a raw sf::Color(...) for anything visible; renderers, maps
// and scenes all refer to the names below. To re-theme the game, edit this file and rebuild.
//
// Colours are grouped by where they appear:
//
//   Palette     the handful of core colours the rest are built from (accent, text, warnings)
//   Panels      backgrounds, outlines and veils behind UI
//   Buttons     menu and station-menu buttons
//   HUD         the flight HUD: dashboard, scanner, markers, tapes, target brackets
//   Maps        the galactic chart and the system map
//   Scenes      overlay text drawn by scenes (system name, hints, messages, countdown)
//   World       3D objects: ships, stations, planets, stars, asteroids
//   Travel      cruise and hyperspace animations
//
// Many entries are defined in terms of the palette (e.g. `scannerStation = accent`), so changing
// `accent` alone re-colours every UI highlight. Override an individual entry to break it away.
//
// Alpha: where a colour is drawn with a fade (streaks, flashes, wireframe shading), the renderer
// replaces its alpha at draw time with withAlpha(); the alpha written here is only the default.
//

#ifndef DUSK_STYLE_H
#define DUSK_STYLE_H

#include <SFML/Graphics/Color.hpp>
#include <algorithm>
#include <cstdint>

namespace style {

    /* ---- Helpers ------------------------------------------------------------------------------ */

    /** The same colour with a different alpha, clamped to 0 (transparent) - 255 (opaque). */
    constexpr sf::Color withAlpha(sf::Color color, int alpha)
    {
        color.a = static_cast<std::uint8_t>(std::clamp(alpha, 0, 255));
        return color;
    }

    /** As above, with alpha as a float (handy for fades computed at draw time). */
    inline sf::Color withAlpha(sf::Color color, float alpha)
    {
        color.a = static_cast<std::uint8_t>(std::clamp(alpha, 0.f, 255.f));
        return color;
    }

    /* ---- Palette ------------------------------------------------------------------------------ */

    /** The UI accent: highlights, selections, readouts, brackets, active bars. */
    inline constexpr sf::Color accent{248, 132, 63};

    /** Flight-assist status: "FA ON" in blue, "FA OFF" in the accent orange. */
    inline constexpr sf::Color assistOn{61, 69, 170};
    inline constexpr sf::Color assistOff = accent;

    /** Text: primary values, secondary detail lines, and dim labels. */
    inline constexpr sf::Color textPrimary = sf::Color{255, 255, 255};
    inline constexpr sf::Color textSecondary{150, 160, 170};
    inline constexpr sf::Color textDim{90, 100, 110};

    /** Faint structure: empty bar tracks, scope grid lines, dashboard edge. */
    inline constexpr sf::Color lineFaint{50, 60, 70};

    /** Something is wrong or backwards: reverse thrust, a target behind you, retrograde. */
    inline constexpr sf::Color warning{240, 90, 90};

    /** Caution, distinct from the accent: the asteroid-field warning. */
    inline constexpr sf::Color caution{255, 214, 92};

    /** "You" and landmarks: own-ship markers, the galactic core. Gold, so it never blends with the accent. */
    inline constexpr sf::Color highlight{255, 210, 120};

    /** Good news: a cheap price, a profit, a trader loading up. Distinct from the orange accent on purpose. */
    inline constexpr sf::Color profit{120, 200, 130};

    /** Clear colour behind everything, and full-screen map backdrops. */
    inline constexpr sf::Color background = sf::Color{0, 0, 0};

    /* ---- Panels ------------------------------------------------------------------------------- */

    /** Side panels on the maps. */
    inline constexpr sf::Color panelFill{8, 12, 18};
    inline constexpr sf::Color panelOutline{60, 70, 78};

    /** The highlighted row in a panel list (accent, mostly transparent, over panelFill). */
    inline constexpr sf::Color panelRowHighlight = withAlpha(accent, 48);

    /** Small boxes behind HUD readouts (heading and pitch values on the tapes). */
    inline constexpr sf::Color readoutBoxFill{8, 12, 18, 220};

    /** Darkening veils behind the main menu and the station menu. */
    inline constexpr sf::Color menuVeil{0, 0, 0, 90};
    inline constexpr sf::Color stationMenuVeil{0, 0, 0, 150};

    /* ---- Buttons ------------------------------------------------------------------------------ */

    /** Primary (selected) buttons are filled with the accent; secondary ones are outlined. */
    inline constexpr sf::Color buttonPrimaryFill = accent;
    inline constexpr sf::Color buttonPrimaryOutline = accent;
    inline constexpr sf::Color buttonPrimaryText = sf::Color{0, 0, 0};
    inline constexpr sf::Color buttonSecondaryFill = sf::Color{0, 0, 0};
    inline constexpr sf::Color buttonSecondaryOutline = accent;
    inline constexpr sf::Color buttonSecondaryText = textPrimary;

    /* ---- HUD ---------------------------------------------------------------------------------- */

    /** Dashboard strip along the bottom of the view. */
    inline constexpr sf::Color dashboardFill{4, 8, 12, 200};
    inline constexpr sf::Color dashboardEdge = lineFaint;

    /** Boresight cross and prograde ring in the view. */
    inline constexpr sf::Color flightMarker = withAlpha(accent, 220);

    /** Retrograde cross (moving backwards). */
    inline constexpr sf::Color retrogradeMarker = warning;

    /** Target brackets, the off-screen target arrow, and the target readout. */
    inline constexpr sf::Color targetMarker = accent;

    /** Throttle bar (forward and reverse) and the speed bar. */
    inline constexpr sf::Color throttleForward = accent;
    inline constexpr sf::Color throttleReverse = warning;
    inline constexpr sf::Color speedBar = textPrimary;

    /** Turn-rate bars and their centre tick. */
    inline constexpr sf::Color rateBar = accent;
    inline constexpr sf::Color rateBarCentre = textPrimary;

    /** The 3D scanner: its ellipse, inner grid and contacts. */
    inline constexpr sf::Color scopeFill{28, 14, 6, 170};
    inline constexpr sf::Color scopeOutline{150, 82, 42};
    inline constexpr sf::Color scopeGrid = lineFaint;
    inline constexpr sf::Color scopeWedge = textDim;
    inline constexpr sf::Color scannerStation = accent;
    inline constexpr sf::Color scannerShip{230, 230, 230};
    inline constexpr sf::Color scannerRock{125, 118, 105};
    inline constexpr sf::Color scannerRockStalk{70, 66, 60};
    inline constexpr sf::Color scannerOwnShip = highlight;

    /** Target compass: dial, and the dot (accent ahead, warning outline behind). */
    inline constexpr sf::Color compassFill = scopeFill;
    inline constexpr sf::Color compassOutline = scopeOutline;
    inline constexpr sf::Color compassAhead = accent;
    inline constexpr sf::Color compassBehind = warning;

    /** Heading and pitch tapes: major ticks, minor ticks and labels, the caret, the zero line. */
    inline constexpr sf::Color tapeMajorTick = textPrimary;
    inline constexpr sf::Color tapeMinorTick = textDim;
    inline constexpr sf::Color tapeCaret = accent;
    inline constexpr sf::Color tapeValue = accent;

    /** Cargo gauge on the dashboard. */
    inline constexpr sf::Color cargoBar = highlight;

    /** "ASTEROID FIELD" above the scanner. */
    inline constexpr sf::Color fieldWarning = caution;

    /** Fuel gauge: normal, and below a fifth of the tank. */
    inline constexpr sf::Color fuelBar = accent;
    inline constexpr sf::Color fuelLow = warning;

    /* ---- Maps --------------------------------------------------------------------------------- */

    /** Galactic chart: range rings, the route line, hover ring, you-are-here diamond, the core. */
    inline constexpr sf::Color chartRangeRing{40, 46, 54};
    inline constexpr sf::Color chartRoute = withAlpha(accent, 140);
    inline constexpr sf::Color chartSelection = accent;
    inline constexpr sf::Color chartHover{200, 200, 200};
    inline constexpr sf::Color chartHere = textPrimary;
    inline constexpr sf::Color chartCore = highlight;

    /** Galactic chart: the jump-range circle around your system (faint fill, accent edge). */
    inline constexpr sf::Color chartRangeFill = withAlpha(accent, 18);
    inline constexpr sf::Color chartRangeEdge = withAlpha(accent, 150);

    /** System dots on the chart, by economy tier. */
    inline constexpr sf::Color economyPoor{130, 130, 130};
    inline constexpr sf::Color economyDeveloping{225, 225, 225};
    inline constexpr sf::Color economyProgressive = accent;

    /** System dots when development isn't known: one neutral colour, so the dots give nothing away. */
    inline constexpr sf::Color chartUnscanned{165, 165, 165};

    /** System map: orbits (and the highlighted one), bodies, the station, traffic, you. */
    inline constexpr sf::Color mapOrbit{55, 62, 70};
    inline constexpr sf::Color mapOrbitHighlight = withAlpha(accent, 120);
    inline constexpr sf::Color mapHighlight = accent;
    inline constexpr sf::Color mapStar = sf::Color{255, 255, 255};
    inline constexpr sf::Color mapPlanet = sf::Color{255, 255, 255};
    inline constexpr sf::Color mapPlanetFill = sf::Color{0, 0, 0};
    inline constexpr sf::Color mapPlanetCross{255, 255, 255, 110};
    inline constexpr sf::Color mapPlanetRing{255, 255, 255, 150};
    inline constexpr sf::Color mapStation = accent;
    inline constexpr sf::Color mapTraffic{200, 200, 200};
    inline constexpr sf::Color mapPlayer = highlight;
    inline constexpr sf::Color mapScaleBar{150, 150, 150};

    /** System map: asteroid belts (dotted edges, dust specks, label) and planet debris-belt halos. */
    inline constexpr sf::Color mapBeltEdge{120, 112, 98};
    inline constexpr sf::Color mapBeltSpeck{150, 140, 120, 170};
    inline constexpr sf::Color mapBeltHalo{200, 185, 150};

    /* ---- Scenes ------------------------------------------------------------------------------- */

    /** System name (top left) and the key-hint column under it. */
    inline constexpr sf::Color systemLabel = textPrimary;
    inline constexpr sf::Color keyHints = textDim;

    /** Docking-computer status line, and centred messages ("ARRIVED IN ...", "HYPERSPACE ABORTED"). */
    inline constexpr sf::Color dockingStatus = accent;
    inline constexpr sf::Color message = textPrimary;

    /** Main menu: save-slot cards and the commander name field. */
    inline constexpr sf::Color slotCardFill{8, 12, 18, 225};

    /** Docking guidance: readings that are fine, borderline and wrong; the slot aperture, the ship's footprint, and the 3D corridor. */
    inline constexpr sf::Color dockingGood = profit;
    inline constexpr sf::Color dockingCaution = caution;
    inline constexpr sf::Color dockingBad = warning;
    inline constexpr sf::Color dockingAperture = textPrimary;
    inline constexpr sf::Color dockingFootprint = accent;
    inline constexpr sf::Color dockingCorridor{110, 200, 130};

    /** Market page: a price below the galaxy average, one above it, and an empty stock. */
    inline constexpr sf::Color marketCheap = profit;
    inline constexpr sf::Color marketDear = caution;
    inline constexpr sf::Color marketSoldOut = textDim;

    /** The highlighted market row or upgrade card while the service list (not the page) has the keyboard. */
    inline constexpr sf::Color marketRowIdle = withAlpha(accent, 22);

    /** Station screen: title, the frame around it, the header strip, the gauge, "coming soon" text. */
    inline constexpr sf::Color stationMenuTitle = textPrimary;
    inline constexpr sf::Color stationFrame = panelFill;
    inline constexpr sf::Color stationFrameEdge = panelOutline;
    inline constexpr sf::Color stationHeader{14, 20, 28};
    inline constexpr sf::Color stationGaugeTrack = lineFaint;
    inline constexpr sf::Color stationComingSoon = textDim;

    /** Hyperspace countdown: title, the big number, and the destination line. */
    inline constexpr sf::Color countdownTitle = textPrimary;
    inline constexpr sf::Color countdownNumber = accent;
    inline constexpr sf::Color countdownDetail = textSecondary;

    /** Text shown inside the hyperspace tunnel. */
    inline constexpr sf::Color tunnelTitle{235, 245, 255};
    inline constexpr sf::Color tunnelDetail{150, 190, 230};

    /* ---- World -------------------------------------------------------------------------------- */

    /** Ship wireframes (player and NPCs). */
    inline constexpr sf::Color shipWireframe = sf::Color{255, 255, 255};

    /** Station wireframe lines run from one colour at their start vertex to another at their end. */
    inline constexpr sf::Color stationLineStart{80, 220, 255};
    inline constexpr sf::Color stationLineEnd{255, 240, 120};

    /** Background stars (alpha is set per star from its brightness). */
    inline constexpr sf::Color starfield = sf::Color{255, 255, 255};

    /** Stellar bodies: the star's filled disc, planet grid lines (alpha shaded by facing), outlines, rings, distant dots. */
    inline constexpr sf::Color starDisc = sf::Color{255, 255, 255};
    inline constexpr sf::Color planetGrid = sf::Color{255, 255, 255};
    inline constexpr sf::Color planetSilhouette{255, 255, 255, 240};
    inline constexpr sf::Color planetRing{255, 255, 255, 150};
    inline constexpr sf::Color distantStarDot = sf::Color{255, 255, 255};
    inline constexpr sf::Color distantPlanetDot{255, 255, 255, 200};

    /** Asteroids: rock wireframes, rocks too small to draw as shapes, and belt dust. */
    inline constexpr sf::Color asteroidLine{210, 205, 195};
    inline constexpr sf::Color asteroidDot{200, 195, 185};
    inline constexpr sf::Color beltDust{120, 115, 105};

    /* ---- Travel ------------------------------------------------------------------------------- */
    //
    // The travel animations keep a cold blue-white, after Star Wars' hyperspace, so they read as
    // something happening to space rather than to the UI.

    /** Cruise motion-blur streaks: bright at the star, fading to nothing along the trail. */
    inline constexpr sf::Color cruiseStreakHead{225, 235, 255};
    inline constexpr sf::Color cruiseStreakTail{150, 190, 255, 0};

    /** Cruise charge (and the end of a hyperspace countdown): converging lines and the tightening ring. */
    inline constexpr sf::Color gatherLineOuter{170, 215, 255, 0};
    inline constexpr sf::Color gatherLineInner{200, 230, 255};
    inline constexpr sf::Color gatherRing{170, 225, 255};

    /** Cruise engage burst and flash; cruise drop-out ring and flash. */
    inline constexpr sf::Color burstLineHead = sf::Color{255, 255, 255};
    inline constexpr sf::Color burstLineTail{160, 210, 255, 0};
    inline constexpr sf::Color engageFlash = sf::Color{255, 255, 255};
    inline constexpr sf::Color dropRing{200, 230, 255};
    inline constexpr sf::Color dropFlash{220, 235, 255};

    /** Hyperspace: stretching stars, the white-outs, the tunnel's background, streaks and rings. */
    inline constexpr sf::Color stretchInner{170, 205, 255};
    inline constexpr sf::Color stretchOuter{240, 248, 255};
    inline constexpr sf::Color hyperspaceFlash{235, 245, 255};
    inline constexpr sf::Color tunnelBackground{3, 7, 22};
    inline constexpr sf::Color tunnelStreakTail{120, 170, 255, 0};
    inline constexpr sf::Color tunnelStreakHead{200, 230, 255};
    inline constexpr sf::Color tunnelRing{235, 245, 255};

} // namespace style

#endif //DUSK_STYLE_H
