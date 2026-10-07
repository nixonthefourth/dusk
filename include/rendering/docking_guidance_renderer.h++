//
// Docking guidance on screen: a corridor of slot-shaped rings leading out of the docking slot in the
// 3D view, and an Apollo-style alignment panel with a scope, readouts and a speed bar.
//
// The scope shows the slot's opening as seen from the cockpit, with the ship's own footprint
// (wingspan by thickness) drawn inside it at the ship's real offset and tilted by its real roll
// error. Docking by eye is then a matter of fitting the orange shape inside the white one.
//

#ifndef DUSK_DOCKING_GUIDANCE_RENDERER_H
#define DUSK_DOCKING_GUIDANCE_RENDERER_H

#include "rendering/projector.h++"
#include "systems/docking_control.h++"
#include "tools/camera.h++"
#include "ui/style.h++"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

class DockingGuidanceRenderer {
public:
    /** Uses the same projection as the rest of the 3D view. */
    explicit DockingGuidanceRenderer(ProjectionConfig projection = {})
        : projector_(projection),
          font_("assets/fonts/Jersey15-Regular.ttf")
    {
    }

    /**
     * Draws the approach corridor: slot-shaped rings at growing distances out from the mouth, joined
     * by rails along their corners, fading with distance. They turn with the slot, so flying down
     * the middle of the rings is flying straight into it.
     */
    void drawCorridor(sf::RenderTarget& target, const docking_control::DockingGuidance& guidance, const Camera& camera) const
    {
        if (!guidance.active || guidance.progress < -40.f)
            return;

        const sf::Vector2u size = target.getSize();
        const Viewport viewport{static_cast<float>(size.x), static_cast<float>(size.y)};
        const Mat4 viewMatrix = projector_.createViewMatrix(camera);

        // Corners of the slot's opening at a distance `distance` out from the mouth.
        const auto corners = [&](float distance)
        {
            const Vec3 centre = guidance.mouth + guidance.normal * distance;
            const Vec3 along = guidance.slotAxis * guidance.halfWidth;
            const Vec3 across = guidance.vertical * guidance.halfHeight;
            return std::array<Vec3, 4>{centre - along - across, centre + along - across, centre + along + across, centre - along + across};
        };

        sf::VertexArray lines(sf::PrimitiveType::Lines);

        const auto addLine = [&](const Vec3& a, const Vec3& b, sf::Color color)
        {
            const auto clipped = projector_.clipLineCameraSpace(transformPoint(viewMatrix, a), transformPoint(viewMatrix, b), camera, viewport);

            if (!clipped)
                return;

            const auto start = projector_.projectCameraSpace(clipped->start, camera, viewport);
            const auto end = projector_.projectCameraSpace(clipped->end, camera, viewport);

            if (!start || !end)
                return;

            lines.append(sf::Vertex{{start->position.x, start->position.y}, color});
            lines.append(sf::Vertex{{end->position.x, end->position.y}, color});
        };

        std::array<Vec3, 4> previous = corners(ringDistances[0]);

        for (std::size_t ring = 0; ring < ringDistances.size(); ++ring)
        {
            // The mouth itself is brightest; the far rings fade out.
            const int alpha = ring == 0 ? 255 : std::max(40, 190 - static_cast<int>(ring) * 16);
            const sf::Color color = style::withAlpha(style::dockingCorridor, alpha);
            const std::array<Vec3, 4> current = corners(ringDistances[ring]);

            for (std::size_t i = 0; i < 4; ++i)
            {
                addLine(current[i], current[(i + 1) % 4], color);

                if (ring > 0)
                    addLine(previous[i], current[i], style::withAlpha(style::dockingCorridor, std::max(30, alpha - 40)));
            }

            previous = current;
        }

        target.draw(lines);
    }

    /**
     * Draws the alignment panel (left side, above the dashboard) and, for the first seconds of
     * docking mode, the key card (top right).
     */
    void drawPanel(sf::RenderTarget& target, const docking_control::DockingGuidance& guidance) const
    {
        if (!guidance.active)
            return;

        drawAlignmentPanel(target, guidance);

        if (guidance.showKeyCard)
            drawKeyCard(target);
    }

private:
    Projector projector_;
    sf::Font font_;

    /** Distances from the mouth, in world units, at which corridor rings are drawn. */
    static constexpr std::array<float, 10> ringDistances = {0.f, 150.f, 300.f, 500.f, 750.f, 1050.f, 1400.f, 1800.f, 2300.f, 2900.f};

    static constexpr float panelX = 14.f;
    static constexpr float panelY = 166.f;
    static constexpr float panelWidth = 232.f;
    static constexpr float panelHeight = 256.f;
    static constexpr float radiansToDegrees = 57.2957795f;

    /** Draws text with its top-left at `position`; returns its width. */
    float drawText(sf::RenderTarget& target, const std::string& string, sf::Vector2f position, unsigned size, sf::Color color, float alignX = 0.f) const
    {
        sf::Text text(font_, string, size);
        text.setFillColor(color);
        const sf::FloatRect bounds = text.getLocalBounds();
        text.setOrigin({bounds.position.x + bounds.size.x * alignX, 0.f});
        text.setPosition(position);
        target.draw(text);
        return bounds.size.x;
    }

    /** A filled or outlined rectangle. */
    static void drawBox(sf::RenderTarget& target, sf::Vector2f position, sf::Vector2f size, sf::Color fill, sf::Color outline, float thickness = 1.f, float rotationDegrees = 0.f)
    {
        sf::RectangleShape box(size);
        box.setOrigin(size * 0.5f);
        box.setPosition(position + size * 0.5f);
        box.setRotation(sf::degrees(rotationDegrees));
        box.setFillColor(fill);
        box.setOutlineColor(outline);
        box.setOutlineThickness(thickness);
        target.draw(box);
    }

    /** Green when `value` is within `good`, yellow within `caution`, red beyond: the colour of a readout. */
    static sf::Color gradeColor(float value, float good, float caution)
    {
        const float magnitude = std::abs(value);
        return magnitude <= good ? style::dockingGood : (magnitude <= caution ? style::dockingCaution : style::dockingBad);
    }

    /** A signed number with one decimal place and a unit, e.g. "+1.2". */
    static std::string signedText(float value, const char* format = "%+.1f")
    {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), format, static_cast<double>(value));
        return buffer;
    }

    /** The scope, the readouts and the speed bar. */
    void drawAlignmentPanel(sf::RenderTarget& target, const docking_control::DockingGuidance& g) const
    {
        drawBox(target, {panelX, panelY}, {panelWidth, panelHeight}, style::readoutBoxFill, style::panelOutline);

        const float x = panelX + 16.f;
        float y = panelY + 8.f;

        // Title row: DOCKING on the left; the permit clock, or who is flying, on the right.
        drawText(target, "DOCKING", {x, y}, 16, style::accent);
        char tag[32];

        if (g.computerFlying)
            std::snprintf(tag, sizeof(tag), "COMPUTER");
        else
            std::snprintf(tag, sizeof(tag), "PERMIT %d:%02d", static_cast<int>(g.permitRemaining) / 60, static_cast<int>(g.permitRemaining) % 60);

        drawText(target, tag, {panelX + panelWidth - 14.f, y + 2.f}, 13, style::textDim, 1.f);
        y += 26.f;

        // ---- Position scope: the slot's opening (white) with the ship's footprint (orange) inside it.
        const sf::Vector2f scopeSize{200.f, 100.f};
        const sf::Vector2f scopeOrigin{x, y};
        const sf::Vector2f centre = scopeOrigin + scopeSize * 0.5f;
        drawBox(target, scopeOrigin, scopeSize, style::panelFill, style::lineFaint);

        // The opening fills 80% of the scope's width; the scale follows from that.
        const float scale = 160.f / std::max(1.f, 2.f * g.halfWidth);
        drawBox(target, {centre.x - g.halfWidth * scale, centre.y - g.halfHeight * scale}, {g.halfWidth * 2.f * scale, g.halfHeight * 2.f * scale}, sf::Color::Transparent, style::dockingAperture);

        // Centre cross.
        sf::VertexArray cross(sf::PrimitiveType::Lines);
        const sf::Color crossColor = style::withAlpha(style::dockingAperture, 90);
        cross.append(sf::Vertex{{centre.x - 8.f, centre.y}, crossColor});
        cross.append(sf::Vertex{{centre.x + 8.f, centre.y}, crossColor});
        cross.append(sf::Vertex{{centre.x, centre.y - 8.f}, crossColor});
        cross.append(sf::Vertex{{centre.x, centre.y + 8.f}, crossColor});
        target.draw(cross);

        // The ship's footprint, at its offset (screen y grows with the cross axis) and tilted by its roll error,
        // and clamped so it never leaves the scope however far off the ship is.
        // Drawn through a view that covers just the scope, so a ship far off the slot is cut off at the
        // scope's edge instead of spilling over the panel.
        const sf::Vector2f footprintSize{2.f * g.shipHalfSpan * scale, 2.f * g.shipHalfThickness * scale};
        const float footprintX = std::clamp(g.offsetU * scale, -scopeSize.x * 0.5f - footprintSize.x * 0.4f, scopeSize.x * 0.5f + footprintSize.x * 0.4f);
        const float footprintY = std::clamp(g.offsetV * scale, -scopeSize.y * 0.5f - footprintSize.y * 2.f, scopeSize.y * 0.5f + footprintSize.y * 2.f);

        const sf::View previousView = target.getView();
        const sf::Vector2u targetSize = target.getSize();
        sf::View scopeView(sf::FloatRect(scopeOrigin, scopeSize));
        scopeView.setViewport(sf::FloatRect({scopeOrigin.x / static_cast<float>(targetSize.x), scopeOrigin.y / static_cast<float>(targetSize.y)},
                                            {scopeSize.x / static_cast<float>(targetSize.x), scopeSize.y / static_cast<float>(targetSize.y)}));
        target.setView(scopeView);
        drawBox(target, {centre.x + footprintX - footprintSize.x * 0.5f, centre.y + footprintY - footprintSize.y * 0.5f}, footprintSize,
                style::withAlpha(style::dockingFootprint, 60), style::dockingFootprint, 1.5f, g.rollError * radiansToDegrees);
        target.setView(previousView);
        y += scopeSize.y + 8.f;

        // ---- Attitude scope: where the nose points, against straight into the slot.
        const sf::Vector2f attitudeSize{64.f, 64.f};
        const sf::Vector2f attitudeCentre{x + attitudeSize.x * 0.5f, y + attitudeSize.y * 0.5f};
        drawBox(target, {x, y}, attitudeSize, style::panelFill, style::lineFaint);

        sf::VertexArray attitudeCross(sf::PrimitiveType::Lines);
        attitudeCross.append(sf::Vertex{{attitudeCentre.x - 28.f, attitudeCentre.y}, style::lineFaint});
        attitudeCross.append(sf::Vertex{{attitudeCentre.x + 28.f, attitudeCentre.y}, style::lineFaint});
        attitudeCross.append(sf::Vertex{{attitudeCentre.x, attitudeCentre.y - 28.f}, style::lineFaint});
        attitudeCross.append(sf::Vertex{{attitudeCentre.x, attitudeCentre.y + 28.f}, style::lineFaint});
        target.draw(attitudeCross);

        // 7 pixels per degree, so the scope spans about +/-4.5 degrees; the dot pins to its edge beyond that.
        const float alignDegreesU = g.alignU * radiansToDegrees;
        const float alignDegreesV = g.alignV * radiansToDegrees;
        const float dotX = std::clamp(alignDegreesU * 7.f, -29.f, 29.f);
        const float dotY = std::clamp(alignDegreesV * 7.f, -29.f, 29.f);
        const sf::Color alignColor = gradeColor(std::max(std::abs(alignDegreesU), std::abs(alignDegreesV)), 1.5f, 4.f);
        sf::CircleShape dot(3.5f);
        dot.setOrigin({3.5f, 3.5f});
        dot.setPosition({attitudeCentre.x + dotX, attitudeCentre.y + dotY});
        dot.setFillColor(alignColor);
        target.draw(dot);
        drawText(target, "NOSE", {attitudeCentre.x, y + attitudeSize.y + 1.f}, 11, style::textDim, 0.5f);

        // ---- Readouts, to the right of the attitude scope.
        const float readX = x + attitudeSize.x + 12.f;
        float readY = y - 2.f;
        const float valueX = panelX + panelWidth - 14.f;
        char buffer[48];

        const auto readout = [&](const char* label, const std::string& value, sf::Color color)
        {
            drawText(target, label, {readX, readY}, 13, style::textDim);
            drawText(target, value, {valueX, readY}, 13, color, 1.f);
            readY += 14.f;
        };

        std::snprintf(buffer, sizeof(buffer), "%.0f", static_cast<double>(std::max(0.f, g.range)));
        readout("RANGE", buffer, style::textPrimary);

        std::snprintf(buffer, sizeof(buffer), "%+.0f / %.0f", static_cast<double>(g.closingSpeed), static_cast<double>(g.speedLimit));
        const bool tooFast = g.closingSpeed > g.speedLimit * 1.05f;
        readout("CLOSING", buffer, tooFast ? style::dockingBad : (g.closingSpeed < -5.f ? style::dockingCaution : style::dockingGood));

        std::snprintf(buffer, sizeof(buffer), "%+.0f / %+.0f", static_cast<double>(g.offsetU), static_cast<double>(g.offsetV));
        readout("OFFSET", buffer, gradeColor(std::max(std::abs(g.offsetU) / std::max(1.f, g.halfWidth - g.shipHalfSpan) * 40.f, std::abs(g.offsetV) / std::max(1.f, g.halfHeight - g.shipHalfThickness) * 40.f), 40.f, 90.f));

        std::snprintf(buffer, sizeof(buffer), "%+.1f / %+.1f", static_cast<double>(alignDegreesU), static_cast<double>(alignDegreesV));
        readout("ALIGN", buffer, alignColor);

        const float rollDegrees = g.rollError * radiansToDegrees;
        std::snprintf(buffer, sizeof(buffer), "%+.1f", static_cast<double>(rollDegrees));
        readout("ROLL ERR", buffer, gradeColor(rollDegrees, 5.f, 15.f));

        std::snprintf(buffer, sizeof(buffer), "%+.0f / %+.0f", static_cast<double>(g.lateralU), static_cast<double>(g.lateralV));
        readout("SLIDE", buffer, gradeColor(std::max(std::abs(g.lateralU), std::abs(g.lateralV)), 6.f, 15.f));
        y += attitudeSize.y + 16.f;

        // ---- Closing-speed bar against the limit: the limit is the bar's end, and the fill turns red beyond it.
        drawText(target, "SPEED", {x, y - 2.f}, 11, style::textDim);
        const float barX = x + 40.f;
        const float barWidth = 160.f;
        drawBox(target, {barX, y + 3.f}, {barWidth, 6.f}, style::lineFaint, sf::Color::Transparent, 0.f);
        const float fraction = g.speedLimit > 0.f ? std::clamp(g.closingSpeed / (g.speedLimit * 1.25f), 0.f, 1.f) : 0.f;
        drawBox(target, {barX, y + 3.f}, {barWidth * fraction, 6.f}, tooFast ? style::dockingBad : style::dockingGood, sf::Color::Transparent, 0.f);

        // A tick at the limit (80% of the way along, since the bar runs to 125% of it).
        drawBox(target, {barX + barWidth * 0.8f - 1.f, y + 1.f}, {2.f, 10.f}, style::textPrimary, sf::Color::Transparent, 0.f);
    }

    /** The key card shown for the first seconds of docking mode. */
    void drawKeyCard(sf::RenderTarget& target) const
    {
        const float width = 258.f;
        const float x = static_cast<float>(target.getSize().x) - width - 12.f;
        const float y = 24.f;
        drawBox(target, {x, y}, {width, 104.f}, style::readoutBoxFill, style::panelOutline);

        drawText(target, "DOCKING MODE", {x + 10.f, y + 5.f}, 14, style::accent);
        const char* lines[] =
        {
            "W/S   THRUST (SPEED VS STATION)",
            "A/D YAW    Q/E PITCH",
            "SHIFT+A/D  SLIDE LEFT / RIGHT",
            "SHIFT+Q/E  SLIDE UP / DOWN",
            "LEFT/RIGHT ARROWS  ROLL",
            "V  AUTO-DOCK    C  CANCEL PERMIT"
        };

        float lineY = y + 24.f;

        for (const char* line : lines)
        {
            drawText(target, line, {x + 10.f, lineY}, 12, style::textSecondary);
            lineY += 13.f;
        }
    }
};

#endif //DUSK_DOCKING_GUIDANCE_RENDERER_H
