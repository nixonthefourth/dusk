//
// Galactic chart: every system in the galaxy, laid out on its spiral, with selection and jumping.
//

#ifndef DUSK_GALAXY_MAP_H
#define DUSK_GALAXY_MAP_H

#include "math/Vec2.h++"
#include "procgen/galaxy.h++"
#include "ui/menu_button.h++"
#include "ui/style.h++"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

/**
 * What the chart is allowed to show about systems beyond the basics (name, position, occupation,
 * planets, stations, belts and traffic). The scanners are paid upgrades; without them the chart
 * shows neither a system's development nor its exports.
 */
struct ChartIntel {
    bool development = false;   // political scanner: Poor / Developing / Progressive
    bool exports = false;       // economics scanner: the goods a system exports
};

/** What the scene should do after the chart has handled an event. */
enum class GalaxyMapAction { None, Close, Jump };

/**
 * Screen-space galactic chart. Owns only view state (selection, zoom, pan); the galaxy itself is
 * passed in, so the chart can never drift out of step with the systems it shows.
 *
 * Controls: arrow keys step to the nearest system in that direction, a click selects, dragging
 * pans, the wheel or +/- zooms, Enter (or the JUMP button) jumps, G or Escape closes.
 */
class GalaxyMap {
public:
    /** Width of the information panel on the right-hand side, in pixels. */
    static constexpr float panelWidth = 250.f;

    /** Resets the view onto the player's current system. */
    void open(const Galaxy& galaxy, int currentIndex)
    {
        selected_ = std::clamp(currentIndex, 0, static_cast<int>(galaxy.systems.size()) - 1);
        hovered_ = -1;
        zoom_ = 2.f;
        viewCenter_ = galaxy.systems[static_cast<std::size_t>(selected_)].mapPosition;
        dragging_ = false;
        mouseDown_ = false;
    }

    /** Index of the currently selected system. */
    int selected() const
    {
        return selected_;
    }

    /** Handles one input event and returns what the scene should do (nothing, close, or jump). */
    GalaxyMapAction handleEvent(
        const sf::Event& event,
        const sf::RenderWindow& window,
        const Galaxy& galaxy,
        int currentIndex,
        bool jumpAvailable,
        float jumpRangeLY
    )
    {
        jumpRangeLY_ = jumpRangeLY;
        const sf::Vector2u size = window.getSize();

        if (const auto* key = event.getIf<sf::Event::KeyPressed>())
        {
            switch (key->code)
            {
                case sf::Keyboard::Key::Escape:
                case sf::Keyboard::Key::G:
                    return GalaxyMapAction::Close;

                case sf::Keyboard::Key::Left: stepSelection(galaxy, {-1.f, 0.f}, size); break;
                case sf::Keyboard::Key::Right: stepSelection(galaxy, {1.f, 0.f}, size); break;
                case sf::Keyboard::Key::Up: stepSelection(galaxy, {0.f, -1.f}, size); break;
                case sf::Keyboard::Key::Down: stepSelection(galaxy, {0.f, 1.f}, size); break;

                case sf::Keyboard::Key::Equal:
                case sf::Keyboard::Key::Add:
                    setZoom(zoom_ * 2.f, galaxy);
                    break;

                case sf::Keyboard::Key::Hyphen:
                case sf::Keyboard::Key::Subtract:
                    setZoom(zoom_ * 0.5f, galaxy);
                    break;

                case sf::Keyboard::Key::Home:
                case sf::Keyboard::Key::H:
                    selected_ = currentIndex;
                    viewCenter_ = galaxy.systems[static_cast<std::size_t>(selected_)].mapPosition;
                    break;

                case sf::Keyboard::Key::Enter:
                case sf::Keyboard::Key::Space:
                    if (canJump(galaxy, currentIndex, jumpAvailable))
                        return GalaxyMapAction::Jump;
                    break;

                default:
                    break;
            }
        }

        if (const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>())
            setZoom(wheel->delta > 0.f ? zoom_ * 2.f : zoom_ * 0.5f, galaxy);

        if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
        {
            if (pressed->button == sf::Mouse::Button::Left)
            {
                const sf::Vector2f mouse = ui::toVector2f(pressed->position);

                if (jumpButtonBounds(size).contains(mouse))
                {
                    if (canJump(galaxy, currentIndex, jumpAvailable))
                        return GalaxyMapAction::Jump;

                    return GalaxyMapAction::None;
                }

                if (mouse.x < mapWidth(size))
                {
                    mouseDown_ = true;
                    dragging_ = false;
                    pressPosition_ = mouse;
                    lastMouse_ = mouse;
                }
            }
        }

        if (const auto* moved = event.getIf<sf::Event::MouseMoved>())
        {
            const sf::Vector2f mouse = ui::toVector2f(moved->position);

            if (mouseDown_)
            {
                const sf::Vector2f fromPress = mouse - pressPosition_;

                if (dragging_ || std::hypot(fromPress.x, fromPress.y) > 5.f)
                {
                    dragging_ = true;
                    const float scale = pixelsPerLightYear(size);
                    viewCenter_ -= Vec2{mouse.x - lastMouse_.x, mouse.y - lastMouse_.y} / scale;
                    clampView();
                }

                lastMouse_ = mouse;
            }

            hovered_ = mouse.x < mapWidth(size) ? systemNear(galaxy, mouse, size) : -1;
        }

        if (const auto* released = event.getIf<sf::Event::MouseButtonReleased>())
        {
            if (released->button == sf::Mouse::Button::Left && mouseDown_)
            {
                if (!dragging_)
                {
                    const int clicked = systemNear(galaxy, ui::toVector2f(released->position), size);

                    if (clicked >= 0)
                        selected_ = clicked;
                }

                mouseDown_ = false;
                dragging_ = false;
            }
        }

        return GalaxyMapAction::None;
    }

    /** Draws the chart full-screen: backdrop, core marker and range rings, every system, then the panel. */
    void draw(
        sf::RenderTarget& target,
        const sf::Font& font,
        const Galaxy& galaxy,
        int currentIndex,
        bool jumpAvailable,
        const std::string& jumpBlockedReason,
        float jumpRangeLY,
        float fuelPerLightYear,
        ChartIntel intel
    )
    {
        jumpRangeLY_ = jumpRangeLY;
        fuelPerLightYear_ = fuelPerLightYear;
        intel_ = intel;
        const sf::Vector2u size = target.getSize();
        const float width = static_cast<float>(size.x);
        const float height = static_cast<float>(size.y);

        sf::RectangleShape backdrop({width, height});
        backdrop.setFillColor(style::background);
        target.draw(backdrop);

        drawCore(target, font, size);
        drawJumpRange(target, galaxy, currentIndex, size);
        drawSystems(target, font, galaxy, currentIndex, size);
        drawPanel(target, font, galaxy, currentIndex, jumpAvailable, jumpBlockedReason, size);
    }

private:
    int selected_ = 0;
    int hovered_ = -1;

    /** Hyperspace reach on the fuel aboard, and fuel burned per light year, as last handed in by the scene. */
    float jumpRangeLY_ = 1e9f;
    float fuelPerLightYear_ = 0.f;

    /** What the commander's scanners let the chart show, as last handed in by the scene. */
    ChartIntel intel_;
    float zoom_ = 2.f;
    Vec2 viewCenter_;

    bool mouseDown_ = false;
    bool dragging_ = false;
    sf::Vector2f pressPosition_;
    sf::Vector2f lastMouse_;

    static constexpr float minZoom = 1.f;
    static constexpr float maxZoom = 16.f;


    /** A jump needs the scene's permission and a destination other than where you already are. */
    bool canJump(const Galaxy& galaxy, int currentIndex, bool jumpAvailable) const
    {
        return jumpAvailable && selected_ != currentIndex && inRange(galaxy, currentIndex, selected_);
    }

    /** True if system `index` is within jump range of the current system on the fuel aboard. */
    bool inRange(const Galaxy& galaxy, int currentIndex, int index) const
    {
        return galacticDistance(
            galaxy.systems[static_cast<std::size_t>(currentIndex)],
            galaxy.systems[static_cast<std::size_t>(index)]
        ) <= jumpRangeLY_;
    }

    /**
     * The jump-range circle around the current system, as on Elite's charts: everything inside it
     * can be reached on the fuel aboard. Drawn as a faint filled disc with an accent edge.
     */
    void drawJumpRange(sf::RenderTarget& target, const Galaxy& galaxy, int currentIndex, sf::Vector2u size) const
    {
        if (jumpRangeLY_ <= 0.f || jumpRangeLY_ > galaxyRadius * 4.f)
            return;

        const sf::Vector2f centre = toScreen(galaxy.systems[static_cast<std::size_t>(currentIndex)].mapPosition, size);
        const float radius = jumpRangeLY_ * pixelsPerLightYear(size);

        sf::CircleShape range(radius, 96);
        range.setOrigin({radius, radius});
        range.setPosition(centre);
        range.setFillColor(style::chartRangeFill);
        range.setOutlineColor(style::chartRangeEdge);
        range.setOutlineThickness(1.5f);
        target.draw(range);
    }

    /** Width of the chart area, left of the info panel. */
    static float mapWidth(sf::Vector2u size)
    {
        return static_cast<float>(size.x) - panelWidth;
    }

    /** Chart scale: at zoom 1 the whole galaxy fits the chart area; each zoom step doubles it. */
    float pixelsPerLightYear(sf::Vector2u size) const
    {
        const float fit = std::min(mapWidth(size), static_cast<float>(size.y)) * 0.46f / galaxyRadius;
        return fit * zoom_;
    }

    sf::Vector2f toScreen(const Vec2& position, sf::Vector2u size) const
    {
        const float scale = pixelsPerLightYear(size);
        return
        {
            mapWidth(size) * 0.5f + (position.x - viewCenter_.x) * scale,
            static_cast<float>(size.y) * 0.5f + (position.y - viewCenter_.y) * scale
        };
    }

    /** Zooms around the selected system, keeping it in view. */
    void setZoom(float zoom, const Galaxy& galaxy)
    {
        zoom_ = std::clamp(zoom, minZoom, maxZoom);
        viewCenter_ = zoom_ <= minZoom ? Vec2{} : galaxy.systems[static_cast<std::size_t>(selected_)].mapPosition;
        clampView();
    }

    /** Keeps the view centre inside the galactic disc, so you can't pan off into nothing. */
    void clampView()
    {
        viewCenter_.x = std::clamp(viewCenter_.x, -galaxyRadius, galaxyRadius);
        viewCenter_.y = std::clamp(viewCenter_.y, -galaxyRadius, galaxyRadius);
    }

    /** Nearest system to a screen point within a small pick radius, or -1. */
    int systemNear(const Galaxy& galaxy, sf::Vector2f point, sf::Vector2u size) const
    {
        constexpr float pickRadius = 12.f;
        int best = -1;
        float bestDistance = pickRadius;

        for (std::size_t index = 0; index < galaxy.systems.size(); ++index)
        {
            const sf::Vector2f screen = toScreen(galaxy.systems[index].mapPosition, size);
            const float distance = std::hypot(screen.x - point.x, screen.y - point.y);

            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = static_cast<int>(index);
            }
        }

        return best;
    }

    /**
     * Moves the selection to the system that best continues in a screen direction: close by, and
     * as straight along that direction as possible. Systems more than 60 degrees off are ignored.
     */
    void stepSelection(const Galaxy& galaxy, Vec2 direction, sf::Vector2u size)
    {
        const Vec2 origin = galaxy.systems[static_cast<std::size_t>(selected_)].mapPosition;
        int best = -1;
        float bestScore = 1e30f;

        for (std::size_t index = 0; index < galaxy.systems.size(); ++index)
        {
            if (static_cast<int>(index) == selected_)
                continue;

            const Vec2 offset = galaxy.systems[index].mapPosition - origin;
            const float distance = length(offset);

            if (distance <= 0.f)
                continue;

            const float alignment = dot(offset / distance, direction);

            if (alignment < 0.5f)
                continue;

            const float score = distance / (alignment * alignment);

            if (score < bestScore)
            {
                bestScore = score;
                best = static_cast<int>(index);
            }
        }

        if (best < 0)
            return;

        selected_ = best;

        // Keep the selection on screen: recentre once it nears the edge of the chart area.
        const sf::Vector2f screen = toScreen(galaxy.systems[static_cast<std::size_t>(best)].mapPosition, size);
        const float margin = 60.f;

        if (screen.x < margin || screen.x > mapWidth(size) - margin ||
            screen.y < margin || screen.y > static_cast<float>(size.y) - margin)
        {
            viewCenter_ = galaxy.systems[static_cast<std::size_t>(best)].mapPosition;
            clampView();
        }
    }

    /** Screen rectangle of the JUMP button near the bottom of the panel. */
    static sf::FloatRect jumpButtonBounds(sf::Vector2u size)
    {
        const float x = static_cast<float>(size.x) - panelWidth + 20.f;
        return {{x, static_cast<float>(size.y) - 112.f}, {panelWidth - 40.f, 44.f}};
    }

    /** Dot colour for a system, by development (set in the stylesheet); one neutral colour without the political scanner. */
    sf::Color systemColor(const SystemInfo& info) const
    {
        if (!intel_.development)
            return style::chartUnscanned;

        switch (info.economyTier)
        {
            case EconomyTier::Poor: return style::economyPoor;
            case EconomyTier::Developing: return style::economyDeveloping;
            case EconomyTier::Progressive: return style::economyProgressive;
        }

        return style::economyDeveloping;
    }

    /** A single one-pixel line. */
    static void drawLine(sf::RenderTarget& target, sf::Vector2f a, sf::Vector2f b, sf::Color color)
    {
        const sf::Vertex line[] = {sf::Vertex(a, color), sf::Vertex(b, color)};
        target.draw(line, 2, sf::PrimitiveType::Lines);
    }

    /** An unfilled circle. */
    static void drawRing(sf::RenderTarget& target, sf::Vector2f center, float radius, sf::Color color, float thickness = 1.f)
    {
        sf::CircleShape ring(radius, 32);
        ring.setOrigin({radius, radius});
        ring.setPosition(center);
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineColor(color);
        ring.setOutlineThickness(thickness);
        target.draw(ring);
    }

    /** Text with its top-left corner at `position`. */
    static void drawLabel(
        sf::RenderTarget& target,
        const sf::Font& font,
        const std::string& string,
        sf::Vector2f position,
        unsigned size,
        sf::Color color
    )
    {
        sf::Text text(font, string, size);
        text.setFillColor(color);
        text.setPosition(position);
        target.draw(text);
    }

    /** Light years with one decimal place, e.g. "7.2 LY". */
    static std::string formatDistance(float lightYears)
    {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.1f LY", lightYears);
        return buffer;
    }

    /** Faint range rings and a marker at the galactic core, the game's end goal. */
    void drawCore(sf::RenderTarget& target, const sf::Font& font, sf::Vector2u size) const
    {
        const sf::Vector2f core = toScreen({}, size);
        const float scale = pixelsPerLightYear(size);

        for (float ring = 100.f; ring <= galaxyRadius; ring += 100.f)
            drawRing(target, core, ring * scale, style::chartRangeRing);

        const sf::Color coreColor = style::chartCore;
        drawLine(target, {core.x - 10.f, core.y}, {core.x + 10.f, core.y}, coreColor);
        drawLine(target, {core.x, core.y - 10.f}, {core.x, core.y + 10.f}, coreColor);
        drawRing(target, core, 5.f, coreColor);
        drawLabel(target, font, "GALACTIC CORE", {core.x + 10.f, core.y + 6.f}, 16, coreColor);
    }

    /**
     * Every system as a dot coloured by economy, then the route line, hover ring, you-are-here
     * diamond and selection reticle on top. Names appear when zoomed right in.
     */
    void drawSystems(
        sf::RenderTarget& target,
        const sf::Font& font,
        const Galaxy& galaxy,
        int currentIndex,
        sf::Vector2u size
    ) const
    {
        const float mapRight = mapWidth(size);
        const float dotRadius = std::clamp(1.2f + zoom_ * 0.35f, 1.5f, 4.f);
        const bool showNames = zoom_ >= 8.f;

        sf::CircleShape dot(dotRadius, 10);
        dot.setOrigin({dotRadius, dotRadius});

        for (std::size_t index = 0; index < galaxy.systems.size(); ++index)
        {
            const SystemInfo& info = galaxy.systems[index];
            const sf::Vector2f screen = toScreen(info.mapPosition, size);

            if (screen.x < -10.f || screen.x > mapRight + 10.f || screen.y < -10.f || screen.y > static_cast<float>(size.y) + 10.f)
                continue;

            dot.setPosition(screen);

            // Out of reach on the fuel aboard: dimmed.
            const bool reachable = galacticDistance(galaxy.systems[static_cast<std::size_t>(currentIndex)], info) <= jumpRangeLY_;
            dot.setFillColor(reachable ? systemColor(info) : style::withAlpha(systemColor(info), 70));
            target.draw(dot);

            if (showNames && screen.x < mapRight - 60.f)
                drawLabel(target, font, info.name, {screen.x + 6.f, screen.y - 8.f}, 14, style::textDim);
        }

        const SystemInfo& current = galaxy.systems[static_cast<std::size_t>(currentIndex)];
        const SystemInfo& selected = galaxy.systems[static_cast<std::size_t>(selected_)];
        const sf::Vector2f currentScreen = toScreen(current.mapPosition, size);
        const sf::Vector2f selectedScreen = toScreen(selected.mapPosition, size);

        if (selected_ != currentIndex)
        {
            drawLine(target, currentScreen, selectedScreen, style::chartRoute);

            const sf::Vector2f middle = (currentScreen + selectedScreen) * 0.5f;
            drawLabel(target, font, formatDistance(galacticDistance(current, selected)), {middle.x + 6.f, middle.y}, 15, style::chartSelection);
        }

        if (hovered_ >= 0 && hovered_ != selected_)
        {
            const sf::Vector2f hoverScreen = toScreen(galaxy.systems[static_cast<std::size_t>(hovered_)].mapPosition, size);
            drawRing(target, hoverScreen, 7.f, style::chartHover);
            drawLabel(target, font, galaxy.systems[static_cast<std::size_t>(hovered_)].name, {hoverScreen.x + 9.f, hoverScreen.y - 9.f}, 16, style::textPrimary);
        }

        // You-are-here diamond.
        sf::CircleShape here(7.f, 4);
        here.setOrigin({7.f, 7.f});
        here.setPosition(currentScreen);
        here.setFillColor(sf::Color::Transparent);
        here.setOutlineColor(style::chartHere);
        here.setOutlineThickness(2.f);
        target.draw(here);

        // Selection reticle.
        const float r = 11.f;
        const float c = 5.f;
        const sf::Vector2f s = selectedScreen;
        drawLine(target, {s.x - r, s.y - r}, {s.x - r + c, s.y - r}, style::chartSelection);
        drawLine(target, {s.x - r, s.y - r}, {s.x - r, s.y - r + c}, style::chartSelection);
        drawLine(target, {s.x + r, s.y - r}, {s.x + r - c, s.y - r}, style::chartSelection);
        drawLine(target, {s.x + r, s.y - r}, {s.x + r, s.y - r + c}, style::chartSelection);
        drawLine(target, {s.x - r, s.y + r}, {s.x - r + c, s.y + r}, style::chartSelection);
        drawLine(target, {s.x - r, s.y + r}, {s.x - r, s.y + r - c}, style::chartSelection);
        drawLine(target, {s.x + r, s.y + r}, {s.x + r - c, s.y + r}, style::chartSelection);
        drawLine(target, {s.x + r, s.y + r}, {s.x + r, s.y + r - c}, style::chartSelection);
        drawLabel(target, font, selected.name, {s.x + 14.f, s.y - 10.f}, 18, style::chartSelection);
    }

    /** The right-hand panel: the selected system's details, its exports, the JUMP button and key hints. */
    void drawPanel(
        sf::RenderTarget& target,
        const sf::Font& font,
        const Galaxy& galaxy,
        int currentIndex,
        bool jumpAvailable,
        const std::string& jumpBlockedReason,
        sf::Vector2u size
    ) const
    {
        const float left = static_cast<float>(size.x) - panelWidth;
        const float height = static_cast<float>(size.y);

        sf::RectangleShape panel({panelWidth, height});
        panel.setPosition({left, 0.f});
        panel.setFillColor(style::panelFill);
        panel.setOutlineColor(style::panelOutline);
        panel.setOutlineThickness(1.f);
        target.draw(panel);

        const SystemInfo& info = galaxy.systems[static_cast<std::size_t>(selected_)];
        const SystemInfo& current = galaxy.systems[static_cast<std::size_t>(currentIndex)];
        const float x = left + 20.f;
        float y = 16.f;

        drawLabel(target, font, "GALACTIC CHART", {x, y}, 18, style::textDim);
        y += 26.f;
        drawLabel(target, font, info.name, {x, y}, 32, style::textPrimary);
        y += 44.f;

        const auto row = [&](const std::string& label, const std::string& value, sf::Color color = style::textPrimary)
        {
            // Long labels ("DEVELOPMENT") shrink to fit the label column instead of running into the value.
            unsigned labelSize = 17;
            const float labelWidth = sf::Text(font, label, labelSize).getLocalBounds().size.x;

            if (labelWidth > 78.f)
                labelSize = static_cast<unsigned>(std::max(11.f, 17.f * 78.f / labelWidth));

            drawLabel(target, font, label, {x, y + (17.f - static_cast<float>(labelSize)) * 0.3f}, labelSize, style::textDim);
            drawLabel(target, font, value, {x + 84.f, y}, 17, color);
            y += 23.f;
        };

        row("DISTANCE", selected_ == currentIndex ? "YOU ARE HERE" : formatDistance(galacticDistance(current, info)), style::accent);
        row("TO CORE", formatDistance(length(info.mapPosition)));

        if (selected_ != currentIndex)
        {
            char fuel[32];
            std::snprintf(fuel, sizeof(fuel), "%.1f t", galacticDistance(current, info) * fuelPerLightYear_);
            row("FUEL NEEDED", fuel, inRange(galaxy, currentIndex, selected_) ? style::textPrimary : style::warning);
        }
        row("DEVELOPMENT", intel_.development ? economyTierName(info.economyTier) : "UNKNOWN",
            intel_.development ? systemColor(info) : style::textDim);
        row("TRADE", info.occupation);
        row("PLANETS", std::to_string(info.planetCount));
        row("STATION", info.stationCount > 0 && info.planetCount > 0 ? "YES" : "NONE");
        row("BELTS", info.beltCount > 0 ? std::to_string(info.beltCount) : "NONE");
        row("TRAFFIC", std::to_string(info.npcShipCount) + " SHIPS");

        y += 6.f;
        drawLabel(target, font, "EXPORTS", {x, y}, 17, style::textDim);
        y += 23.f;

        if (intel_.exports)
        {
            for (const std::string& good : info.goods)
            {
                if (y > height - 150.f)
                    break;

                drawLabel(target, font, good, {x + 10.f, y}, 17, style::textPrimary);
                y += 21.f;
            }
        }
        else
        {
            drawLabel(target, font, "UNKNOWN", {x + 10.f, y}, 17, style::textDim);
            y += 24.f;
        }

        // Say what would reveal what's hidden, so the player knows there is something to buy.
        if (!intel_.exports || !intel_.development)
        {
            y += 6.f;

            if (!intel_.exports)
            {
                drawLabel(target, font, "ECONOMICS SCANNER: EXPORTS", {x, y}, 13, style::textDim);
                y += 17.f;
            }

            if (!intel_.development)
            {
                drawLabel(target, font, "POLITICAL SCANNER: DEVELOPMENT", {x, y}, 13, style::textDim);
                y += 17.f;
            }

            drawLabel(target, font, "SOLD AT STATIONS (UPGRADES)", {x, y}, 13, style::textDim);
        }

        const sf::FloatRect button = jumpButtonBounds(size);
        const bool enabled = canJump(galaxy, currentIndex, jumpAvailable);
        std::string blocked = jumpBlockedReason;

        if (selected_ == currentIndex)
            blocked = "CURRENT SYSTEM";
        else if (jumpAvailable && !inRange(galaxy, currentIndex, selected_))
            blocked = "OUT OF RANGE";

        sf::Text jumpText(font, enabled ? "JUMP" : blocked, enabled ? 28 : 18);
        ui::drawButton(target, button, jumpText, enabled, enabled);

        drawLabel(target, font, "ARROWS / CLICK  select", {x, height - 58.f}, 15, style::textDim);
        drawLabel(target, font, "WHEEL / +-  zoom   DRAG  pan", {x, height - 40.f}, 15, style::textDim);
        drawLabel(target, font, "ENTER  jump   H  home   G  close", {x, height - 22.f}, 15, style::textDim);
    }
};

#endif //DUSK_GALAXY_MAP_H
