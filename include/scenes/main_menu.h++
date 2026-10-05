//
// Created by Mykyta Khomiakov on 24/07/2026.
//
// The main menu: the player's ship turning slowly in a showcase orbit behind three screens.
//
//   Title   PLAY / EXIT
//   Slots   the three save slots: load a commander, start a new game in an empty slot, or delete one
//   Name    "COMMANDER ____": type a name for a new commander, then start
//

#ifndef DUSK_MAIN_MENU_H
#define DUSK_MAIN_MENU_H

#include "scenes/scene.h++"
#include "systems/save_game.h++"
#include "ui/menu_button.h++"
#include "ui/style.h++"
#include <SFML/Graphics.hpp>
#include <array>
#include <cstdio>
#include <optional>
#include <string>

class MainMenuScene : public Scene {
public:
    /** Places the player's ship at the origin with a larger showcase scale, and styles the button labels. */
    MainMenuScene()
        : font_("assets/fonts/Jersey15-Regular.ttf"),
          playText_(font_, "PLAY", 42),
          exitText_(font_, "EXIT", 42)
    {
        world_.playerShip.position = {0.f, -80.f, 0.f};
        world_.playerShip.yaw = 0.55f;

        ObjLoadOptions options;
        options.scale = 200.f;
        options.rotationDegrees = {0.f, -90.f, -90.f};
        options.centerOnOrigin = true;
        world_.playerShip.loadObjModel("assets/objects/ships/banshee.obj", options);

        playText_.setStyle(sf::Text::Bold);
        playText_.setFillColor(style::buttonPrimaryText);

        exitText_.setStyle(sf::Text::Bold);
        exitText_.setFillColor(style::buttonSecondaryText);
    }

    /** Name used in logs and debugging. */
    const char* name() const override
    {
        return "main menu";
    }

    /** The menu's own small world: just the ship, no system. */
    World& world() override
    {
        return world_;
    }

    /** Read-only access to the menu's world. */
    const World& world() const override
    {
        return world_;
    }

    /** Routes input to whichever screen is showing. */
    void handleEvent(const sf::Event& event, const sf::RenderWindow& window) override
    {
        switch (screen_)
        {
            case Screen::Title: handleTitleEvent(event, window); break;
            case Screen::Slots: handleSlotsEvent(event, window); break;
            case Screen::Name: handleNameEvent(event, window); break;
        }
    }

    /** Escape steps back a screen (and cancels a pending delete) instead of quitting, except on the title. */
    bool capturesEscape() const override
    {
        return screen_ != Screen::Title;
    }

    /** The ship on the title screen is for show; it never takes flight input. */
    bool acceptsShipInput() const override
    {
        return false;
    }

    /** No flight HUD on the title screen. */
    bool showsHud() const override
    {
        return false;
    }

    /** A slow, wide showcase orbit around the ship. */
    void updateCamera(Camera& camera, float dt, ShipCameraRig& rig) override
    {
        ShipCameraSettings settings;
        settings.showcaseDistance = 2200.f;
        settings.showcaseHeight = 900.f;
        settings.showcaseSpeed = 0.28f;
        updateCameraToShowcaseShip(camera, world_.playerShip, dt, rig, settings);
    }

    /** Darkens the scene slightly and draws the current screen over it. */
    void drawOverlay(sf::RenderTarget& target) override
    {
        const sf::Vector2u size = target.getSize();

        sf::RectangleShape veil({static_cast<float>(size.x), static_cast<float>(size.y)});
        veil.setFillColor(style::menuVeil);
        target.draw(veil);

        switch (screen_)
        {
            case Screen::Title:
                ui::drawButton(target, playButtonBounds(size), playText_, true, playHovered_);
                ui::drawButton(target, exitButtonBounds(size), exitText_, false, exitHovered_);
                break;

            case Screen::Slots: drawSlots(target); break;
            case Screen::Name: drawNameEntry(target); break;
        }
    }

    /** Hands any requested transition to main.cpp exactly once, then clears it. */
    SceneTransition consumeTransition() override
    {
        const SceneTransition transition = pendingTransition_;
        pendingTransition_ = SceneTransition::None;
        return transition;
    }

    /** The game to start (set when a slot is loaded or a new commander is named), handed over once. */
    std::optional<GameLaunch> consumeGameLaunch() override
    {
        std::optional<GameLaunch> launch = pendingLaunch_;
        pendingLaunch_.reset();
        return launch;
    }

private:
    enum class Screen { Title, Slots, Name };

    World world_;
    sf::Font font_;
    sf::Text playText_;
    sf::Text exitText_;
    bool playHovered_ = false;
    bool exitHovered_ = false;
    SceneTransition pendingTransition_ = SceneTransition::None;
    std::optional<GameLaunch> pendingLaunch_;

    Screen screen_ = Screen::Title;

    /** What's in each slot, read from disk whenever the slot screen opens or a slot is deleted. */
    std::array<std::optional<SaveGame>, saveSlotCount> slots_;

    /** Slot screen: the highlighted row (0-2 are slots, 3 is BACK), and a slot awaiting delete confirmation (-1 if none). */
    int selectedRow_ = 0;
    int confirmDeleteSlot_ = -1;

    /** Name screen: which empty slot the new commander goes in, and what's been typed so far. */
    int newGameSlot_ = 0;
    std::string typedName_;

    /** Seed every new game uses; matches the galaxy main.cpp builds. */
    static constexpr std::uint32_t newGameGalaxySeed = 1337u;

    static constexpr int backRow = saveSlotCount;

    /* ---- Title ------------------------------------------------------------------------------- */

    /** Enter/Space or clicking PLAY opens the slot screen; clicking EXIT quits. */
    void handleTitleEvent(const sf::Event& event, const sf::RenderWindow& window)
    {
        if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>())
        {
            if (keyPressed->code == sf::Keyboard::Key::Enter || keyPressed->code == sf::Keyboard::Key::Space)
                openSlots();
        }

        if (const auto* mouseMoved = event.getIf<sf::Event::MouseMoved>())
            updateHoverState(window.getSize(), ui::toVector2f(mouseMoved->position));

        if (const auto* mousePressed = event.getIf<sf::Event::MouseButtonPressed>())
        {
            if (mousePressed->button != sf::Mouse::Button::Left)
                return;

            const sf::Vector2f mouse = ui::toVector2f(mousePressed->position);

            if (playButtonBounds(window.getSize()).contains(mouse))
                openSlots();
            else if (exitButtonBounds(window.getSize()).contains(mouse))
                pendingTransition_ = SceneTransition::Exit;
        }
    }

    /** Screen rectangle of the PLAY button (top of the stack). */
    static sf::FloatRect playButtonBounds(sf::Vector2u targetSize)
    {
        return ui::stackedButtonBounds(targetSize, 0, 2);
    }

    /** Screen rectangle of the EXIT button (bottom of the stack). */
    static sf::FloatRect exitButtonBounds(sf::Vector2u targetSize)
    {
        return ui::stackedButtonBounds(targetSize, 1, 2);
    }

    /** Thickens the outline of whichever title button the mouse is over. */
    void updateHoverState(sf::Vector2u targetSize, sf::Vector2f mouse)
    {
        playHovered_ = playButtonBounds(targetSize).contains(mouse);
        exitHovered_ = exitButtonBounds(targetSize).contains(mouse);
    }

    /* ---- Slots ------------------------------------------------------------------------------- */

    /** Reads all three slots from disk and shows the slot screen. */
    void openSlots()
    {
        for (int slot = 0; slot < saveSlotCount; ++slot)
            slots_[static_cast<std::size_t>(slot)] = readSave(slot);

        screen_ = Screen::Slots;
        selectedRow_ = 0;
        confirmDeleteSlot_ = -1;
    }

    /** Loads an occupied slot, or starts naming a new commander for an empty one. */
    void activateSlot(int slot)
    {
        const auto& saved = slots_[static_cast<std::size_t>(slot)];

        if (saved)
        {
            GameLaunch launch;
            launch.slot = slot;
            launch.save = *saved;
            launch.isNewGame = false;
            pendingLaunch_ = launch;
            pendingTransition_ = SceneTransition::EnterSystem;
            return;
        }

        newGameSlot_ = slot;
        typedName_.clear();
        screen_ = Screen::Name;
    }

    /**
     * Deleting is two steps: the first request marks the slot (its card asks for confirmation),
     * the second deletes it. Any other key or click cancels.
     */
    void requestDelete(int slot)
    {
        if (!slots_[static_cast<std::size_t>(slot)])
            return;

        if (confirmDeleteSlot_ == slot)
        {
            deleteSave(slot);
            slots_[static_cast<std::size_t>(slot)] = readSave(slot);
            confirmDeleteSlot_ = -1;
            return;
        }

        confirmDeleteSlot_ = slot;
    }

    /**
     * Up/Down choose a slot (or BACK), Enter loads it or starts a new game, D or Delete asks to
     * delete it (press again to confirm), Escape goes back. The mouse can click a card, its DELETE
     * button, or BACK.
     */
    void handleSlotsEvent(const sf::Event& event, const sf::RenderWindow& window)
    {
        const sf::Vector2u size = window.getSize();

        if (const auto* key = event.getIf<sf::Event::KeyPressed>())
        {
            const bool deleteKey = key->code == sf::Keyboard::Key::D || key->code == sf::Keyboard::Key::Delete;

            if (!deleteKey && key->code != sf::Keyboard::Key::Enter)
                confirmDeleteSlot_ = -1;

            switch (key->code)
            {
                case sf::Keyboard::Key::Escape:
                    if (confirmDeleteSlot_ < 0)
                        screen_ = Screen::Title;
                    confirmDeleteSlot_ = -1;
                    break;

                case sf::Keyboard::Key::Up:
                case sf::Keyboard::Key::W:
                    selectedRow_ = (selectedRow_ + backRow) % (backRow + 1);
                    break;

                case sf::Keyboard::Key::Down:
                case sf::Keyboard::Key::S:
                case sf::Keyboard::Key::Tab:
                    selectedRow_ = (selectedRow_ + 1) % (backRow + 1);
                    break;

                case sf::Keyboard::Key::Enter:
                case sf::Keyboard::Key::Space:
                    if (confirmDeleteSlot_ >= 0 && confirmDeleteSlot_ == selectedRow_)
                        requestDelete(selectedRow_); // Enter also confirms a pending delete
                    else if (selectedRow_ == backRow)
                        screen_ = Screen::Title;
                    else
                        activateSlot(selectedRow_);
                    break;

                case sf::Keyboard::Key::D:
                case sf::Keyboard::Key::Delete:
                    if (selectedRow_ < backRow)
                        requestDelete(selectedRow_);
                    break;

                default:
                    break;
            }
        }

        if (const auto* moved = event.getIf<sf::Event::MouseMoved>())
        {
            const sf::Vector2f mouse = ui::toVector2f(moved->position);

            for (int row = 0; row <= backRow; ++row)
            {
                if (slotRowBounds(size, row).contains(mouse))
                    selectedRow_ = row;
            }
        }

        if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
        {
            if (pressed->button != sf::Mouse::Button::Left)
                return;

            const sf::Vector2f mouse = ui::toVector2f(pressed->position);

            for (int slot = 0; slot < saveSlotCount; ++slot)
            {
                if (slots_[static_cast<std::size_t>(slot)] && deleteButtonBounds(size, slot).contains(mouse))
                {
                    requestDelete(slot);
                    return;
                }
            }

            confirmDeleteSlot_ = -1;

            if (slotRowBounds(size, backRow).contains(mouse))
            {
                screen_ = Screen::Title;
                return;
            }

            for (int slot = 0; slot < saveSlotCount; ++slot)
            {
                if (slotRowBounds(size, slot).contains(mouse))
                {
                    activateSlot(slot);
                    return;
                }
            }
        }
    }

    static constexpr float cardWidth = 460.f;
    static constexpr float cardHeight = 84.f;
    static constexpr float cardGap = 14.f;

    /** Card for slot `row` (0-2), or the BACK button (row 3), centred on screen. */
    static sf::FloatRect slotRowBounds(sf::Vector2u size, int row)
    {
        const float width = std::min(cardWidth, static_cast<float>(size.x) - 40.f);
        const float totalHeight = static_cast<float>(saveSlotCount) * (cardHeight + cardGap) + 52.f;
        const float top = (static_cast<float>(size.y) - totalHeight) * 0.5f + 24.f;
        const float left = (static_cast<float>(size.x) - width) * 0.5f;

        if (row == backRow)
            return {{left + width * 0.5f - 90.f, top + static_cast<float>(saveSlotCount) * (cardHeight + cardGap) + 6.f}, {180.f, 46.f}};

        return {{left, top + static_cast<float>(row) * (cardHeight + cardGap)}, {width, cardHeight}};
    }

    /** The small DELETE button on an occupied slot's card. */
    static sf::FloatRect deleteButtonBounds(sf::Vector2u size, int slot)
    {
        const sf::FloatRect card = slotRowBounds(size, slot);
        return {{card.position.x + card.size.x - 104.f, card.position.y + card.size.y - 34.f}, {92.f, 24.f}};
    }

    /** Text with its top-left at `position`, or aligned by `alignX` (0 left, 0.5 centre, 1 right). */
    void drawText(sf::RenderTarget& target, const std::string& string, sf::Vector2f position, unsigned size, sf::Color color, float alignX = 0.f) const
    {
        sf::Text text(font_, string, size);
        text.setFillColor(color);
        const sf::FloatRect bounds = text.getLocalBounds();
        text.setOrigin({bounds.position.x + bounds.size.x * alignX, 0.f});
        text.setPosition(position);
        target.draw(text);
    }

    /** The three slot cards and BACK. Occupied cards show the commander and where and when they saved. */
    void drawSlots(sf::RenderTarget& target) const
    {
        const sf::Vector2u size = target.getSize();
        const float centreX = static_cast<float>(size.x) * 0.5f;
        drawText(target, "SELECT COMMANDER", {centreX, slotRowBounds(size, 0).position.y - 46.f}, 30, style::textPrimary, 0.5f);

        for (int slot = 0; slot < saveSlotCount; ++slot)
        {
            const sf::FloatRect card = slotRowBounds(size, slot);
            const bool selected = selectedRow_ == slot;
            const auto& saved = slots_[static_cast<std::size_t>(slot)];

            sf::RectangleShape box(card.size);
            box.setPosition(card.position);
            box.setFillColor(style::slotCardFill);
            box.setOutlineColor(selected ? style::accent : style::panelOutline);
            box.setOutlineThickness(selected ? 2.f : 1.f);
            target.draw(box);

            const float x = card.position.x + 16.f;
            const float y = card.position.y + 10.f;
            drawText(target, "SLOT " + std::to_string(slot + 1), {x, y}, 15, style::textDim);

            if (!saved)
            {
                drawText(target, "EMPTY", {x, y + 20.f}, 26, style::textSecondary);
                drawText(target, "NEW GAME", {card.position.x + card.size.x - 16.f, y + 26.f}, 20, selected ? style::accent : style::textDim, 1.f);
                continue;
            }

            drawText(target, "COMMANDER " + saved->commanderName, {x, y + 18.f}, 26, selected ? style::accent : style::textPrimary);

            char detail[128];
            std::snprintf(detail, sizeof(detail), "%s   %.1f CR   %s",
                          saved->systemName.empty() ? "UNKNOWN SYSTEM" : saved->systemName.c_str(),
                          saved->credits,
                          formatPlayTime(saved->playTimeSeconds).c_str());
            drawText(target, detail, {x, y + 50.f}, 16, style::textSecondary);
            drawText(target, saved->savedAt, {card.position.x + card.size.x - 16.f, y}, 14, style::textDim, 1.f);

            // DELETE button, turning into a confirmation prompt on the first press.
            const sf::FloatRect button = deleteButtonBounds(size, slot);
            const bool confirming = confirmDeleteSlot_ == slot;
            sf::RectangleShape deleteBox(button.size);
            deleteBox.setPosition(button.position);
            deleteBox.setFillColor(confirming ? style::warning : sf::Color::Transparent);
            deleteBox.setOutlineColor(style::warning);
            deleteBox.setOutlineThickness(1.f);
            target.draw(deleteBox);
            drawText(target, confirming ? "CONFIRM?" : "DELETE", {button.position.x + button.size.x * 0.5f, button.position.y + 3.f}, 15,
                     confirming ? style::buttonPrimaryText : style::warning, 0.5f);
        }

        sf::Text back(font_, "BACK", 24);
        ui::drawButton(target, slotRowBounds(size, backRow), back, selectedRow_ == backRow, selectedRow_ == backRow);

        drawText(target, "UP/DOWN  select     ENTER  load / new game     D  delete     ESC  back",
                 {centreX, static_cast<float>(size.y) - 30.f}, 15, style::textDim, 0.5f);
    }

    /* ---- Naming a new commander -------------------------------------------------------------- */

    /** The name field's box, centred on screen. */
    static sf::FloatRect nameFieldBounds(sf::Vector2u size)
    {
        const float width = std::min(460.f, static_cast<float>(size.x) - 40.f);
        return {{(static_cast<float>(size.x) - width) * 0.5f, static_cast<float>(size.y) * 0.5f - 30.f}, {width, 60.f}};
    }

    /** START and BACK under the name field. */
    static sf::FloatRect nameButtonBounds(sf::Vector2u size, int index)
    {
        const sf::FloatRect field = nameFieldBounds(size);
        const float width = (field.size.x - 16.f) * 0.5f;
        return {{field.position.x + static_cast<float>(index) * (width + 16.f), field.position.y + field.size.y + 24.f}, {width, 48.f}};
    }

    /** Starts the new game in the chosen slot with the typed name (JAMES if left empty). */
    void startNewGame()
    {
        GameLaunch launch;
        launch.slot = newGameSlot_;
        launch.save = newSaveGame(typedName_, newGameGalaxySeed);
        launch.isNewGame = true;
        pendingLaunch_ = launch;
        pendingTransition_ = SceneTransition::EnterSystem;
    }

    /**
     * Typing fills the name (letters, digits, space, hyphen, apostrophe and full stop, shown in
     * capitals, up to 16 characters); Backspace deletes; Enter starts; Escape goes back to the slots.
     */
    void handleNameEvent(const sf::Event& event, const sf::RenderWindow& window)
    {
        if (const auto* typed = event.getIf<sf::Event::TextEntered>())
        {
            const char32_t c = typed->unicode;

            if (c < 128 && isCommanderNameCharacter(static_cast<char>(c)) && typedName_.size() < maxCommanderNameLength)
            {
                // No leading spaces and no double spaces, so what you see is what gets saved.
                if (!(c == U' ' && (typedName_.empty() || typedName_.back() == ' ')))
                    typedName_ += static_cast<char>(std::toupper(static_cast<int>(c)));
            }
        }

        if (const auto* key = event.getIf<sf::Event::KeyPressed>())
        {
            switch (key->code)
            {
                case sf::Keyboard::Key::Backspace:
                    if (!typedName_.empty())
                        typedName_.pop_back();
                    break;

                case sf::Keyboard::Key::Enter:
                    startNewGame();
                    break;

                case sf::Keyboard::Key::Escape:
                    screen_ = Screen::Slots;
                    break;

                default:
                    break;
            }
        }

        if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
        {
            if (pressed->button != sf::Mouse::Button::Left)
                return;

            const sf::Vector2f mouse = ui::toVector2f(pressed->position);

            if (nameButtonBounds(window.getSize(), 0).contains(mouse))
                startNewGame();
            else if (nameButtonBounds(window.getSize(), 1).contains(mouse))
                screen_ = Screen::Slots;
        }
    }

    /** "NEW COMMANDER", the slot, the name field with a blinking caret, and START / BACK. */
    void drawNameEntry(sf::RenderTarget& target) const
    {
        const sf::Vector2u size = target.getSize();
        const sf::FloatRect field = nameFieldBounds(size);
        const float centreX = static_cast<float>(size.x) * 0.5f;

        drawText(target, "NEW COMMANDER", {centreX, field.position.y - 96.f}, 34, style::textPrimary, 0.5f);
        drawText(target, "SLOT " + std::to_string(newGameSlot_ + 1) + "  -  ENTER YOUR NAME", {centreX, field.position.y - 50.f}, 17, style::textDim, 0.5f);

        sf::RectangleShape box(field.size);
        box.setPosition(field.position);
        box.setFillColor(style::slotCardFill);
        box.setOutlineColor(style::accent);
        box.setOutlineThickness(2.f);
        target.draw(box);

        // "COMMANDER" label, then the typed name (or a dim placeholder), then a blinking caret.
        const float textY = field.position.y + 13.f;
        sf::Text label(font_, "COMMANDER ", 28);
        label.setFillColor(style::textDim);
        label.setPosition({field.position.x + 16.f, textY});
        target.draw(label);

        const float nameX = field.position.x + 16.f + label.getLocalBounds().size.x + 10.f;
        sf::Text nameText(font_, typedName_.empty() ? "JAMES" : typedName_, 28);
        nameText.setFillColor(typedName_.empty() ? style::textDim : style::accent);
        nameText.setPosition({nameX, textY});
        target.draw(nameText);

        const bool caretOn = static_cast<int>(world_.elapsedTime * 2.0) % 2 == 0;

        if (caretOn)
        {
            const float caretX = typedName_.empty() ? nameX : nameX + nameText.getLocalBounds().size.x + 4.f;
            sf::RectangleShape caret({3.f, 30.f});
            caret.setPosition({caretX, textY + 2.f});
            caret.setFillColor(style::accent);
            target.draw(caret);
        }

        sf::Text start(font_, "START", 24);
        sf::Text back(font_, "BACK", 24);
        ui::drawButton(target, nameButtonBounds(size, 0), start, true, false);
        ui::drawButton(target, nameButtonBounds(size, 1), back, false, false);

        drawText(target, "Leave it blank to fly as JAMESON.   ENTER  start     ESC  back",
                 {centreX, static_cast<float>(size.y) - 30.f}, 15, style::textDim, 0.5f);
    }
};

#endif //DUSK_MAIN_MENU_H
