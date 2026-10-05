//
// Created by Mykyta Khomiakov on 24/07/2026.
//
// The title screen: the player's ship turning slowly in a showcase orbit, with PLAY and EXIT buttons.
//

#ifndef DUSK_MAIN_MENU_H
#define DUSK_MAIN_MENU_H

#include "scenes/scene.h++"
#include "ui/menu_button.h++"
#include "ui/style.h++"
#include <SFML/Graphics.hpp>

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

    /** Enter/Space or clicking PLAY starts the game; clicking EXIT quits. Mouse movement updates hover states. */
    void handleEvent(const sf::Event& event, const sf::RenderWindow& window) override
    {
        if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>())
        {
            if (keyPressed->code == sf::Keyboard::Key::Enter ||
                keyPressed->code == sf::Keyboard::Key::Space)
            {
                pendingTransition_ = SceneTransition::EnterSystem;
            }
        }

        if (const auto* mouseMoved = event.getIf<sf::Event::MouseMoved>())
            updateHoverState(window.getSize(), ui::toVector2f(mouseMoved->position));

        if (const auto* mousePressed = event.getIf<sf::Event::MouseButtonPressed>())
        {
            if (mousePressed->button != sf::Mouse::Button::Left)
                return;

            const sf::Vector2f mouse = ui::toVector2f(mousePressed->position);

            if (playButtonBounds(window.getSize()).contains(mouse))
            {
                pendingTransition_ = SceneTransition::EnterSystem;
                return;
            }

            if (exitButtonBounds(window.getSize()).contains(mouse))
                pendingTransition_ = SceneTransition::Exit;
        }
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

    /** Darkens the scene slightly and draws the PLAY and EXIT buttons. */
    void drawOverlay(sf::RenderTarget& target) override
    {
        const sf::Vector2u size = target.getSize();
        const sf::Vector2f viewport =
        {
            static_cast<float>(size.x),
            static_cast<float>(size.y)
        };

        sf::RectangleShape veil(viewport);
        veil.setFillColor(style::menuVeil);
        target.draw(veil);

        ui::drawButton(target, playButtonBounds(size), playText_, true, playHovered_);
        ui::drawButton(target, exitButtonBounds(size), exitText_, false, exitHovered_);
    }

    /** Hands any requested transition to main.cpp exactly once, then clears it. */
    SceneTransition consumeTransition() override
    {
        const SceneTransition transition = pendingTransition_;
        pendingTransition_ = SceneTransition::None;
        return transition;
    }

private:
    World world_;
    sf::Font font_;
    sf::Text playText_;
    sf::Text exitText_;
    bool playHovered_ = false;
    bool exitHovered_ = false;
    SceneTransition pendingTransition_ = SceneTransition::None;

    /** Screen rectangle of the PLAY button (top of the stack). */
    static sf::FloatRect playButtonBounds(sf::Vector2u targetSize)
    {
        return buttonBounds(targetSize, 0);
    }

    /** Screen rectangle of the EXIT button (bottom of the stack). */
    static sf::FloatRect exitButtonBounds(sf::Vector2u targetSize)
    {
        return buttonBounds(targetSize, 1);
    }

    /** Rectangle of button `index` in a centred stack of two. */
    static sf::FloatRect buttonBounds(sf::Vector2u targetSize, int index)
    {
        return ui::stackedButtonBounds(targetSize, index, 2);
    }

    /** Thickens the outline of whichever button the mouse is over. */
    void updateHoverState(sf::Vector2u targetSize, sf::Vector2f mouse)
    {
        playHovered_ = playButtonBounds(targetSize).contains(mouse);
        exitHovered_ = exitButtonBounds(targetSize).contains(mouse);
    }
};

#endif //DUSK_MAIN_MENU_H
