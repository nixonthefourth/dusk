//
// The station screen shown while docked: a list of services on the left, the selected service's
// page on the right. Refuelling, the market, upgrades and saving work today; missions and the garage
// are laid out as "coming soon" pages, so adding one later means filling in its page rather than
// new plumbing.
//

#ifndef DUSK_STATION_MENU_H
#define DUSK_STATION_MENU_H

#include "systems/refuelling.h++"
#include "systems/upgrades.h++"
#include "ui/menu_button.h++"
#include "ui/style.h++"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <vector>

/** Every service the station screen can show. Add a value here and a row in stationServices to add a page. */
enum class StationPage { Refuel, Market, Outfitting, Missions, Garage, SaveGame, Launch };

/** One entry in the service list. */
struct StationService {
    StationPage page;
    const char* label;

    /** False for services that aren't built yet; they still have a page explaining what's coming. */
    bool available;

    /** What the page says while the service is unavailable (or a short description when it is). */
    const char* blurb;
};

/** The services, in list order. LAUNCH stays last. */
inline const std::array<StationService, 7> stationServices =
{{
    {StationPage::Refuel, "REFUEL", true, "Top up the tank. Fuel is priced by the local economy."},
    {StationPage::Market, "MARKET", true, "Buy and sell trade goods. Each system sells what it makes cheap and pays more for what it lacks."},
    {StationPage::Outfitting, "UPGRADES", true, "Cargo bays, fuel tanks and a docking computer."},
    {StationPage::Missions, "MISSIONS", false, "Take on courier runs, deliveries and contracts for credits."},
    {StationPage::Garage, "GARAGE", false, "Store, swap and buy ships."},
    {StationPage::SaveGame, "SAVE GAME", true, "Save your progress to this commander's slot, load the last save, or return to the main menu."},
    {StationPage::Launch, "LAUNCH", true, "Undock and fly out of the slot."},
}};

/** What the scene should do after the station screen handled an event. */
enum class StationMenuAction
{
    None, Close, Launch,
    RefuelFull, RefuelOneTonne,
    SaveGame, LoadGame, MainMenu,
    BuyGoodOne, BuyGoodMax, SellGoodOne, SellGoodAll,   // act on StationMenu::selectedGood()
    InstallUpgrade                                      // acts on StationMenu::selectedUpgrade()
};

/** One line of the market table: a good, what it costs and pays here, and what you hold of it. */
struct MarketRowView {
    std::string name;
    float buyPrice = 0.f;       // what the next tonne costs you
    float sellPrice = 0.f;      // what the next tonne pays you
    int stock = 0;              // tonnes the station has
    int held = 0;               // tonnes in your hold
    float versusAverage = 0.f;  // price against the galaxy average: -0.25 is 25% cheaper
    bool canBuy = false;        // stock, hold room and credits all allow at least a tonne
    bool canSell = false;       // you hold some
};

/** One line of the upgrades page: what it is, and whether (and for how much) you can fit it. */
struct UpgradeRowView {
    std::string name;
    std::string description;
    double listPrice = 0.0;
    UpgradeOffer offer;         // status, and the net cost after trading in what's fitted
};

/**
 * Everything the station screen displays, gathered by the scene each frame. The menu only reads
 * this; buying fuel happens in the scene (via refuelling.h++) when the menu returns an action.
 */
struct StationMenuView {
    std::string title;          // e.g. "JOREL MINOR STATION"
    std::string commanderName;
    double credits = 0.0;

    float fuel = 0.f;
    float fuelCapacity = 0.f;
    float hullMass = 0.f;
    float totalMass = 0.f;
    float jumpRangeLY = 0.f;
    float fullTankRangeLY = 0.f;
    float pricePerTonne = 0.f;

    RefuelQuote fillQuote;      // what "FILL TANK" would buy
    RefuelQuote oneTonneQuote;  // what "BUY 1 t" would buy

    /** The hold: tonnes carried, hold size, the fitted bay, and the market and upgrade rows. */
    int cargoUsed = 0;
    int cargoCapacity = 0;
    std::string cargoModuleName;
    std::vector<MarketRowView> market;
    std::vector<UpgradeRowView> upgrades;

    /** What a trader agent last did here ("TRADER LOADED 9 t WINES  (40 s ago)"), or empty. */
    std::string traderNote;

    /** The save slot this game belongs to (0-2, or -1 for none), and what's currently saved in it. */
    int saveSlot = -1;
    bool hasSave = false;
    std::string savedAt;
    std::string savedSystem;
    std::string playTime;
};

/**
 * The station screen. Owns only its selection; everything it shows comes in through a
 * StationMenuView. Keys: Up/Down choose a service, Enter does the page's main action (fill the
 * tank, save, launch) or, on the market and upgrades pages, opens the page's list. In a list,
 * Up/Down choose a row, B or Enter buys (Shift for the most you can), S sells (Shift for all),
 * and Left, Tab or Escape return to the services. L launches from anywhere, and Escape in the
 * service list closes the screen (you stay docked). Buttons take mouse clicks.
 */
class StationMenu {
public:
    /** Reopens on the first service each time you dock. */
    void open()
    {
        selected_ = 0;
        hoveredButton_ = -1;
        pageFocused_ = false;
        selectedGood_ = 0;
        selectedUpgrade_ = 0;
    }

    /** The highlighted good (an index into the goods list) and upgrade (an index into the catalogue). */
    int selectedGood() const
    {
        return selectedGood_;
    }

    /** The highlighted upgrade, an index into upgradeCatalogue(). */
    int selectedUpgrade() const
    {
        return selectedUpgrade_;
    }

    /** The page of the currently selected service. */
    StationPage selectedPage() const
    {
        return stationServices[static_cast<std::size_t>(selected_)].page;
    }

    /** Handles one event and returns what the scene should do. */
    StationMenuAction handleEvent(const sf::Event& event, const sf::RenderWindow& window)
    {
        const sf::Vector2u size = window.getSize();
        const int count = static_cast<int>(stationServices.size());

        if (const auto* key = event.getIf<sf::Event::KeyPressed>())
        {
            // Launching works from anywhere, including inside a list.
            if (key->code == sf::Keyboard::Key::L)
                return StationMenuAction::Launch;

            if (pageFocused_)
                return handlePageKey(*key);

            switch (key->code)
            {
                case sf::Keyboard::Key::Escape: return StationMenuAction::Close;

                case sf::Keyboard::Key::Up:
                case sf::Keyboard::Key::W:
                    selected_ = (selected_ + count - 1) % count;
                    break;

                case sf::Keyboard::Key::Down:
                case sf::Keyboard::Key::S:
                case sf::Keyboard::Key::Tab:
                    selected_ = (selected_ + 1) % count;
                    break;

                case sf::Keyboard::Key::B:
                    if (selectedPage() == StationPage::Refuel)
                        return StationMenuAction::RefuelOneTonne;
                    break;

                case sf::Keyboard::Key::Right:
                    if (pageRowCount() > 0)
                        pageFocused_ = true;
                    break;

                case sf::Keyboard::Key::Enter:
                case sf::Keyboard::Key::Space:
                    // List pages open for browsing; everything else does its main action.
                    if (pageRowCount() > 0)
                        pageFocused_ = true;
                    else
                        return primaryAction();
                    break;

                default:
                    break;
            }
        }

        if (const auto* moved = event.getIf<sf::Event::MouseMoved>())
        {
            const sf::Vector2f mouse = ui::toVector2f(moved->position);
            hoveredButton_ = pageButtonAt(size, mouse);
        }

        if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
        {
            if (pressed->button != sf::Mouse::Button::Left)
                return StationMenuAction::None;

            const sf::Vector2f mouse = ui::toVector2f(pressed->position);

            for (int index = 0; index < count; ++index)
            {
                if (serviceBounds(size, index).contains(mouse))
                {
                    selected_ = index;
                    pageFocused_ = false;
                    return StationMenuAction::None;
                }
            }

            // A click on a market or upgrade row selects it and gives the list the keyboard.
            for (int row = 0; row < pageRowCount(); ++row)
            {
                if (rowBounds(size, row).contains(mouse))
                {
                    (selectedPage() == StationPage::Market ? selectedGood_ : selectedUpgrade_) = row;
                    pageFocused_ = true;
                    return StationMenuAction::None;
                }
            }

            const int button = pageButtonAt(size, mouse);

            if (button >= 0)
                return buttonAction(button);
        }

        return StationMenuAction::None;
    }

    /** Draws the whole screen: veil, frame, header, service list, the selected page and key hints. */
    void draw(sf::RenderTarget& target, const sf::Font& font, const StationMenuView& view) const
    {
        const sf::Vector2u size = target.getSize();
        const sf::FloatRect frame = frameBounds(size);

        sf::RectangleShape veil({static_cast<float>(size.x), static_cast<float>(size.y)});
        veil.setFillColor(style::stationMenuVeil);
        target.draw(veil);

        sf::RectangleShape panel(frame.size);
        panel.setPosition(frame.position);
        panel.setFillColor(style::stationFrame);
        panel.setOutlineColor(style::stationFrameEdge);
        panel.setOutlineThickness(1.f);
        target.draw(panel);

        drawHeader(target, font, view, frame);
        drawServiceList(target, font, size);
        drawPage(target, font, view, size);

        std::string hints = "UP/DOWN  service     ENTER  confirm     L  launch     ESC  close";

        if (pageFocused_ && selectedPage() == StationPage::Market)
            hints = "UP/DOWN  good     B  buy 1 (SHIFT: max)     S  sell 1 (SHIFT: all)     LEFT  back     L  launch";
        else if (pageFocused_)
            hints = "UP/DOWN  choose     ENTER  install     LEFT  back     L  launch";
        else if (pageRowCount() > 0)
            hints = "UP/DOWN  service     ENTER  open list     L  launch     ESC  close";

        drawText(target, font, hints, {frame.position.x + 16.f, frame.position.y + frame.size.y - 26.f}, 15, style::textDim);
    }

private:
    int selected_ = 0;
    int hoveredButton_ = -1;

    /** True while a list page (market, upgrades) has the keyboard instead of the service list. */
    bool pageFocused_ = false;

    /** The highlighted good on the market page and upgrade on the upgrades page. */
    int selectedGood_ = 0;
    int selectedUpgrade_ = 0;

    static constexpr float headerHeight = 64.f;
    static constexpr float listWidth = 190.f;
    static constexpr float serviceHeight = 40.f;
    static constexpr float serviceGap = 8.f;

    /** The main action of the selected page: fill the tank, or launch. Others have none yet. */
    StationMenuAction primaryAction() const
    {
        return buttonAction(0);
    }

    /** Rows in the selected page's list: the goods on the market page, the catalogue on upgrades, else none. */
    int pageRowCount() const
    {
        switch (selectedPage())
        {
            case StationPage::Market: return goodCount;
            case StationPage::Outfitting: return static_cast<int>(upgradeCatalogue().size());
            default: return 0;
        }
    }

    /** Keys while a list page has the keyboard: move between rows, buy, sell, or step back out. */
    StationMenuAction handlePageKey(const sf::Event::KeyPressed& key)
    {
        const int rows = pageRowCount();
        int& row = selectedPage() == StationPage::Market ? selectedGood_ : selectedUpgrade_;

        switch (key.code)
        {
            case sf::Keyboard::Key::Escape:
            case sf::Keyboard::Key::Left:
            case sf::Keyboard::Key::Tab:
                pageFocused_ = false;
                break;

            case sf::Keyboard::Key::Up:
                row = (row + rows - 1) % rows;
                break;

            case sf::Keyboard::Key::Down:
                row = (row + 1) % rows;
                break;

            case sf::Keyboard::Key::B:
            case sf::Keyboard::Key::Enter:
            case sf::Keyboard::Key::Space:
                if (selectedPage() == StationPage::Market)
                    return key.shift ? StationMenuAction::BuyGoodMax : StationMenuAction::BuyGoodOne;

                return StationMenuAction::InstallUpgrade;

            case sf::Keyboard::Key::S:
                if (selectedPage() == StationPage::Market)
                    return key.shift ? StationMenuAction::SellGoodAll : StationMenuAction::SellGoodOne;
                break;

            default:
                break;
        }

        return StationMenuAction::None;
    }

    /** What each of the selected page's buttons does, left to right. */
    StationMenuAction buttonAction(int button) const
    {
        switch (selectedPage())
        {
            case StationPage::Market:
                if (button == 0) return StationMenuAction::BuyGoodOne;
                if (button == 1) return StationMenuAction::BuyGoodMax;
                if (button == 2) return StationMenuAction::SellGoodOne;
                return StationMenuAction::SellGoodAll;

            case StationPage::Outfitting:
                return StationMenuAction::InstallUpgrade;

            case StationPage::Refuel:
                return button == 0 ? StationMenuAction::RefuelFull : StationMenuAction::RefuelOneTonne;

            case StationPage::SaveGame:
                if (button == 0) return StationMenuAction::SaveGame;
                if (button == 1) return StationMenuAction::LoadGame;
                return StationMenuAction::MainMenu;

            case StationPage::Launch:
                return StationMenuAction::Launch;

            default:
                return StationMenuAction::None;
        }
    }

    /* ---- Layout -------------------------------------------------------------------------------- */

    /** The screen's frame, inset from the window edges. */
    static sf::FloatRect frameBounds(sf::Vector2u size)
    {
        const float inset = 28.f;
        return {{inset, inset}, {static_cast<float>(size.x) - inset * 2.f, static_cast<float>(size.y) - inset * 2.f}};
    }

    /** Button for service `index` in the left-hand list. */
    static sf::FloatRect serviceBounds(sf::Vector2u size, int index)
    {
        const sf::FloatRect frame = frameBounds(size);
        const float top = frame.position.y + headerHeight + 18.f;
        return {{frame.position.x + 16.f, top + static_cast<float>(index) * (serviceHeight + serviceGap)}, {listWidth, serviceHeight}};
    }

    /** Where the selected page is drawn: right of the list, under the header. */
    static sf::FloatRect pageBounds(sf::Vector2u size)
    {
        const sf::FloatRect frame = frameBounds(size);
        const float left = frame.position.x + 16.f + listWidth + 24.f;
        const float top = frame.position.y + headerHeight + 18.f;
        return {{left, top}, {frame.position.x + frame.size.x - 16.f - left, frame.size.y - headerHeight - 70.f}};
    }

    /** Height of one row in the market table, and where the table starts on the page (under the title and column heads). */
    static constexpr float marketRowHeight = 21.f;
    static constexpr float tableTop = 66.f;
    static constexpr float upgradeRowHeight = 46.f;
    static constexpr float upgradeRowGap = 6.f;

    /** Row `index` of the selected page's list: a market line or an upgrade card. */
    sf::FloatRect rowBounds(sf::Vector2u size, int index) const
    {
        const sf::FloatRect page = pageBounds(size);

        if (selectedPage() == StationPage::Market)
            return {{page.position.x, page.position.y + tableTop + static_cast<float>(index) * marketRowHeight}, {page.size.x, marketRowHeight}};

        return {{page.position.x, page.position.y + 44.f + static_cast<float>(index) * (upgradeRowHeight + upgradeRowGap)}, {page.size.x, upgradeRowHeight}};
    }

    /** The page's action buttons, side by side along its bottom, sharing the width: 0 is the main action. */
    sf::FloatRect pageButtonBounds(sf::Vector2u size, int index) const
    {
        const sf::FloatRect page = pageBounds(size);
        const int count = std::max(1, pageButtonCount());
        const float width = std::min(220.f, (page.size.x - 16.f * static_cast<float>(count - 1)) / static_cast<float>(count));
        return {{page.position.x + static_cast<float>(index) * (width + 16.f), page.position.y + page.size.y - 52.f}, {width, 44.f}};
    }

    /** How many action buttons the selected page has. */
    int pageButtonCount() const
    {
        switch (selectedPage())
        {
            case StationPage::Refuel: return 2;
            case StationPage::Market: return 4;
            case StationPage::Outfitting: return 1;
            case StationPage::SaveGame: return 3;
            case StationPage::Launch: return 1;
            default: return 0;
        }
    }

    /** Index of the page button under `point`, or -1. */
    int pageButtonAt(sf::Vector2u size, sf::Vector2f point) const
    {
        for (int index = 0; index < pageButtonCount(); ++index)
        {
            if (pageButtonBounds(size, index).contains(point))
                return index;
        }

        return -1;
    }

    /* ---- Drawing ------------------------------------------------------------------------------- */

    static float drawText(
        sf::RenderTarget& target,
        const sf::Font& font,
        const std::string& string,
        sf::Vector2f position,
        unsigned size,
        sf::Color color,
        float alignX = 0.f
    )
    {
        sf::Text text(font, string, size);
        text.setFillColor(color);
        const sf::FloatRect bounds = text.getLocalBounds();
        text.setOrigin({bounds.position.x + bounds.size.x * alignX, 0.f});
        text.setPosition(position);
        target.draw(text);
        return bounds.size.x;
    }

    /** Credits with one decimal place, Elite style: "1000.0 CR". */
    static std::string formatCredits(double credits)
    {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.1f CR", credits);
        return buffer;
    }

    /** Tonnes with one decimal place: "2.1 t". */
    static std::string formatTonnes(float tonnes)
    {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.1f t", tonnes);
        return buffer;
    }

    /** Title and commander on the left; credits and fuel on the right. */
    static void drawHeader(sf::RenderTarget& target, const sf::Font& font, const StationMenuView& view, const sf::FloatRect& frame)
    {
        sf::RectangleShape header({frame.size.x, headerHeight});
        header.setPosition(frame.position);
        header.setFillColor(style::stationHeader);
        target.draw(header);

        const float x = frame.position.x + 16.f;
        const float right = frame.position.x + frame.size.x - 16.f;
        drawText(target, font, view.title, {x, frame.position.y + 6.f}, 30, style::stationMenuTitle);
        drawText(target, font, "COMMANDER " + view.commanderName, {x, frame.position.y + 40.f}, 15, style::textDim);

        drawText(target, font, formatCredits(view.credits), {right, frame.position.y + 10.f}, 22, style::accent, 1.f);

        char fuel[96];
        std::snprintf(fuel, sizeof(fuel), "FUEL %.1f / %.1f t     CARGO %d / %d t", view.fuel, view.fuelCapacity, view.cargoUsed, view.cargoCapacity);
        drawText(target, font, fuel, {right, frame.position.y + 38.f}, 16, style::textSecondary, 1.f);
    }

    /** The service list: the selected one filled with the accent, unavailable ones dimmed and tagged. */
    void drawServiceList(sf::RenderTarget& target, const sf::Font& font, sf::Vector2u size) const
    {
        for (int index = 0; index < static_cast<int>(stationServices.size()); ++index)
        {
            const StationService& service = stationServices[static_cast<std::size_t>(index)];
            const sf::FloatRect bounds = serviceBounds(size, index);
            const bool selected = index == selected_;

            sf::RectangleShape button(bounds.size);
            button.setPosition(bounds.position);
            button.setFillColor(selected ? style::buttonPrimaryFill : style::buttonSecondaryFill);
            button.setOutlineColor(service.available ? style::buttonSecondaryOutline : style::panelOutline);
            button.setOutlineThickness(selected ? 2.f : 1.f);
            target.draw(button);

            const sf::Color labelColor = selected
                ? style::buttonPrimaryText
                : (service.available ? style::buttonSecondaryText : style::stationComingSoon);
            drawText(target, font, service.label, {bounds.position.x + 14.f, bounds.position.y + 7.f}, 22, labelColor);

            if (!service.available)
                drawText(target, font, "SOON", {bounds.position.x + bounds.size.x - 10.f, bounds.position.y + 13.f}, 13,
                         selected ? style::buttonPrimaryText : style::stationComingSoon, 1.f);
        }
    }

    /** One of the page's action buttons, filled when hovered. */
    void drawPageButton(sf::RenderTarget& target, const sf::Font& font, sf::Vector2u size, int index, const std::string& label, bool enabled) const
    {
        sf::Text text(font, label, 20);
        ui::drawButton(target, pageButtonBounds(size, index), text, enabled && (index == 0 || hoveredButton_ == index), hoveredButton_ == index);
    }

    /** Wraps a sentence into lines of at most `width` pixels and draws them; returns the y below the last line. */
    static float drawParagraph(sf::RenderTarget& target, const sf::Font& font, const std::string& textString, sf::Vector2f position, float width, unsigned size, sf::Color color)
    {
        std::string line;
        std::string word;
        float y = position.y;

        const auto flush = [&]()
        {
            drawText(target, font, line, {position.x, y}, size, color);
            y += static_cast<float>(size) + 6.f;
            line.clear();
        };

        for (std::size_t i = 0; i <= textString.size(); ++i)
        {
            const char c = i < textString.size() ? textString[i] : ' ';

            if (c != ' ')
            {
                word += c;
                continue;
            }

            const std::string candidate = line.empty() ? word : line + " " + word;

            if (!line.empty() && sf::Text(font, candidate, size).getLocalBounds().size.x > width)
            {
                flush();
                line = word;
            }
            else
            {
                line = candidate;
            }

            word.clear();
        }

        if (!line.empty())
            flush();

        return y;
    }

    /** The selected service's page: refuelling, launch, or a "coming soon" description. */
    void drawPage(sf::RenderTarget& target, const sf::Font& font, const StationMenuView& view, sf::Vector2u size) const
    {
        const StationService& service = stationServices[static_cast<std::size_t>(selected_)];
        const sf::FloatRect page = pageBounds(size);
        const float x = page.position.x;
        float y = page.position.y;

        drawText(target, font, service.label, {x, y}, 28, style::accent);
        y += 40.f;

        switch (service.page)
        {
            case StationPage::Refuel:
                drawRefuelPage(target, font, view, size, page, y);
                return;

            case StationPage::Launch:
                drawParagraph(target, font, service.blurb, {x, y}, page.size.x, 18, style::textSecondary);
                drawPageButton(target, font, size, 0, "LAUNCH", true);
                return;

            case StationPage::Market:
                drawMarketPage(target, font, view, size, page);
                return;

            case StationPage::Outfitting:
                drawUpgradesPage(target, font, view, size, page);
                return;

            case StationPage::SaveGame:
                drawSavePage(target, font, view, size, page, y);
                return;

            default:
                drawText(target, font, "COMING SOON", {x, y}, 20, style::stationComingSoon);
                drawParagraph(target, font, service.blurb, {x, y + 32.f}, page.size.x, 18, style::textSecondary);
                return;
        }
    }

    /**
     * The market: one line per good with what it costs and pays here, the station's stock, your
     * hold, and the price against the galaxy average (green when cheap, yellow when dear), then
     * your hold and mass, the last thing a trader did here, and BUY / SELL buttons.
     */
    void drawMarketPage(sf::RenderTarget& target, const sf::Font& font, const StationMenuView& view, sf::Vector2u size, const sf::FloatRect& page) const
    {
        const float x = page.position.x;
        const float headY = page.position.y + tableTop - 22.f;

        drawText(target, font, "MARKET", {x, page.position.y}, 28, style::accent);

        // Column heads, then the right-hand edge of each numeric column.
        constexpr float buyEdge = 250.f, sellEdge = 312.f, stockEdge = 372.f, holdEdge = 424.f, avgEdge = 490.f;
        const sf::Color head = style::textDim;
        drawText(target, font, "GOOD", {x + 8.f, headY}, 14, head);
        drawText(target, font, "BUY", {x + buyEdge, headY}, 14, head, 1.f);
        drawText(target, font, "SELL", {x + sellEdge, headY}, 14, head, 1.f);
        drawText(target, font, "STOCK", {x + stockEdge, headY}, 14, head, 1.f);
        drawText(target, font, "HOLD", {x + holdEdge, headY}, 14, head, 1.f);
        drawText(target, font, "VS AVG", {x + avgEdge, headY}, 14, head, 1.f);

        char buffer[32];

        for (int row = 0; row < static_cast<int>(view.market.size()); ++row)
        {
            const MarketRowView& line = view.market[static_cast<std::size_t>(row)];
            const sf::FloatRect bounds = rowBounds(size, row);
            const bool selected = row == selectedGood_;

            if (selected)
            {
                sf::RectangleShape bar(bounds.size);
                bar.setPosition(bounds.position);
                bar.setFillColor(pageFocused_ ? style::panelRowHighlight : style::marketRowIdle);
                target.draw(bar);
            }

            const float y = bounds.position.y + 2.f;
            drawText(target, font, line.name, {x + 8.f, y}, 16, selected && pageFocused_ ? style::accent : style::textPrimary);

            if (line.stock >= 1)
            {
                std::snprintf(buffer, sizeof(buffer), "%.1f", static_cast<double>(line.buyPrice));
                drawText(target, font, buffer, {x + buyEdge, y}, 16, style::textPrimary, 1.f);
            }
            else
            {
                drawText(target, font, "SOLD OUT", {x + buyEdge, y}, 14, style::marketSoldOut, 1.f);
            }

            std::snprintf(buffer, sizeof(buffer), "%.1f", static_cast<double>(line.sellPrice));
            drawText(target, font, buffer, {x + sellEdge, y}, 16, style::textSecondary, 1.f);

            drawText(target, font, std::to_string(line.stock), {x + stockEdge, y}, 16, style::textDim, 1.f);
            drawText(target, font, line.held > 0 ? std::to_string(line.held) : "-", {x + holdEdge, y}, 16, line.held > 0 ? style::accent : style::textDim, 1.f);

            std::snprintf(buffer, sizeof(buffer), "%+.0f%%", static_cast<double>(line.versusAverage) * 100.0);
            const sf::Color avgColor = line.versusAverage <= -0.05f ? style::marketCheap
                : (line.versusAverage >= 0.05f ? style::marketDear : style::textSecondary);
            drawText(target, font, buffer, {x + avgEdge, y}, 16, avgColor, 1.f);
        }

        // Under the table: the hold and mass, and the latest trader sighting.
        const float infoY = page.position.y + tableTop + static_cast<float>(view.market.size()) * marketRowHeight + 10.f;
        std::snprintf(buffer, sizeof(buffer), "HOLD  %d / %d t", view.cargoUsed, view.cargoCapacity);
        const float held = drawText(target, font, buffer, {x + 8.f, infoY}, 17, style::cargoBar);
        std::snprintf(buffer, sizeof(buffer), "SHIP MASS  %.1f t", static_cast<double>(view.totalMass));
        drawText(target, font, buffer, {x + 8.f + held + 28.f, infoY}, 17, style::textSecondary);
        drawText(target, font, view.traderNote.empty() ? "NO TRADER HAS CALLED HERE YET" : view.traderNote,
                 {x + 8.f, infoY + 22.f}, 15, view.traderNote.empty() ? style::textDim : style::profit);

        const MarketRowView* current = selectedGood_ < static_cast<int>(view.market.size()) ? &view.market[static_cast<std::size_t>(selectedGood_)] : nullptr;
        const bool canBuy = current && current->canBuy;
        const bool canSell = current && current->canSell;
        drawPageButton(target, font, size, 0, "BUY 1", canBuy);
        drawPageButton(target, font, size, 1, "BUY MAX", canBuy);
        drawPageButton(target, font, size, 2, "SELL 1", canSell);
        drawPageButton(target, font, size, 3, "SELL ALL", canSell);
    }

    /**
     * The outfitting page: each upgrade as a compact card with its description and what it would
     * cost you (after the trade-in on whatever it replaces), a one-line reminder of the rules, and
     * an INSTALL button. Cards for what you already have show INSTALLED.
     */
    void drawUpgradesPage(sf::RenderTarget& target, const sf::Font& font, const StationMenuView& view, sf::Vector2u size, const sf::FloatRect& page) const
    {
        const float x = page.position.x;
        drawText(target, font, "UPGRADES", {x, page.position.y}, 28, style::accent);

        for (int row = 0; row < static_cast<int>(view.upgrades.size()); ++row)
        {
            const UpgradeRowView& upgrade = view.upgrades[static_cast<std::size_t>(row)];
            const sf::FloatRect bounds = rowBounds(size, row);
            const bool selected = row == selectedUpgrade_;

            sf::RectangleShape card(bounds.size);
            card.setPosition(bounds.position);
            card.setFillColor(selected ? (pageFocused_ ? style::panelRowHighlight : style::marketRowIdle) : style::stationHeader);
            card.setOutlineColor(selected && pageFocused_ ? style::accent : style::panelOutline);
            card.setOutlineThickness(selected && pageFocused_ ? 2.f : 1.f);
            target.draw(card);

            const bool usable = upgrade.offer.status == UpgradeStatus::Available || upgrade.offer.status == UpgradeStatus::CantAfford;
            drawText(target, font, upgrade.name, {bounds.position.x + 12.f, bounds.position.y + 4.f}, 19, usable ? style::textPrimary : style::textDim);
            drawText(target, font, upgrade.description, {bounds.position.x + 12.f, bounds.position.y + 26.f}, 14, style::textSecondary);

            // Right-hand side: the price you'd pay, or why you can't.
            const float right = bounds.position.x + bounds.size.x - 12.f;
            char buffer[64];

            switch (upgrade.offer.status)
            {
                case UpgradeStatus::Installed:
                    drawText(target, font, "INSTALLED", {right, bounds.position.y + 12.f}, 18, style::profit, 1.f);
                    break;

                case UpgradeStatus::HaveBetter:
                    drawText(target, font, "YOU HAVE BETTER", {right, bounds.position.y + 14.f}, 14, style::textDim, 1.f);
                    break;

                case UpgradeStatus::Available:
                case UpgradeStatus::CantAfford:
                {
                    const bool affordable = upgrade.offer.status == UpgradeStatus::Available;
                    std::snprintf(buffer, sizeof(buffer), "%.0f CR", upgrade.offer.cost);
                    drawText(target, font, buffer, {right, bounds.position.y + 3.f}, 20, affordable ? style::accent : style::warning, 1.f);

                    if (upgrade.offer.cost < upgrade.listPrice - 0.5)
                        drawText(target, font, "AFTER TRADE-IN", {right, bounds.position.y + 27.f}, 12, style::textDim, 1.f);
                    else if (!affordable)
                        drawText(target, font, "NOT ENOUGH CREDITS", {right, bounds.position.y + 27.f}, 12, style::warning, 1.f);

                    break;
                }
            }
        }

        const float noteY = rowBounds(size, static_cast<int>(view.upgrades.size())).position.y + 2.f;
        const std::string note = "KEEPS " + std::to_string(static_cast<int>(upgradeReserveCredits)) + " CR IN RESERVE   REPLACED MODULES TRADE IN AT HALF PRICE";
        drawText(target, font, note, {x, noteY}, 14, style::textDim);

        const UpgradeRowView* current = selectedUpgrade_ < static_cast<int>(view.upgrades.size()) ? &view.upgrades[static_cast<std::size_t>(selectedUpgrade_)] : nullptr;
        drawPageButton(target, font, size, 0, "INSTALL", current && current->offer.status == UpgradeStatus::Available);
    }

    /** This commander's slot and what's saved in it, with SAVE / LOAD / MAIN MENU. */
    void drawSavePage(sf::RenderTarget& target, const sf::Font& font, const StationMenuView& view, sf::Vector2u size, const sf::FloatRect& page, float y) const
    {
        const float x = page.position.x;

        const auto row = [&](const std::string& label, const std::string& value)
        {
            drawText(target, font, label, {x, y}, 17, style::textDim);
            drawText(target, font, value, {x + 150.f, y}, 17, style::textPrimary);
            y += 24.f;
        };

        if (view.saveSlot < 0)
        {
            drawParagraph(target, font, "This game isn't attached to a save slot, so it can't be saved. Start a game from the main menu to pick a slot.",
                          {x, y}, page.size.x, 17, style::textSecondary);
            drawPageButton(target, font, size, 2, "MAIN MENU", true);
            return;
        }

        row("SLOT", std::to_string(view.saveSlot + 1));
        row("COMMANDER", view.commanderName);

        if (view.hasSave)
        {
            row("LAST SAVED", view.savedAt);
            row("SAVED AT", view.savedSystem);
            row("PLAY TIME", view.playTime);
        }
        else
        {
            row("LAST SAVED", "NEVER");
        }

        y += 8.f;
        drawParagraph(target, font,
                      "SAVE writes your credits, fuel and this station to the slot. LOAD and MAIN MENU discard anything since your last save.",
                      {x, y}, page.size.x, 16, style::textSecondary);

        drawPageButton(target, font, size, 0, "SAVE", true);
        drawPageButton(target, font, size, 1, "LOAD", view.hasSave);
        drawPageButton(target, font, size, 2, "MAIN MENU", true);
    }

    /** The fuel gauge, range, mass and price, and the FILL TANK / BUY 1 t buttons. */
    void drawRefuelPage(sf::RenderTarget& target, const sf::Font& font, const StationMenuView& view, sf::Vector2u size, const sf::FloatRect& page, float y) const
    {
        const float x = page.position.x;
        const float gaugeWidth = std::min(page.size.x, 360.f);
        const float fraction = view.fuelCapacity > 0.f ? std::clamp(view.fuel / view.fuelCapacity, 0.f, 1.f) : 0.f;

        sf::RectangleShape track({gaugeWidth, 16.f});
        track.setPosition({x, y});
        track.setFillColor(style::stationGaugeTrack);
        target.draw(track);

        sf::RectangleShape fill({gaugeWidth * fraction, 16.f});
        fill.setPosition({x, y});
        fill.setFillColor(fraction < 0.2f ? style::fuelLow : style::fuelBar);
        target.draw(fill);
        y += 30.f;

        const auto row = [&](const std::string& label, const std::string& value, sf::Color color = style::textPrimary)
        {
            drawText(target, font, label, {x, y}, 17, style::textDim);
            drawText(target, font, value, {x + 150.f, y}, 17, color);
            y += 24.f;
        };

        char buffer[64];
        row("FUEL", formatTonnes(view.fuel) + " / " + formatTonnes(view.fuelCapacity));

        std::snprintf(buffer, sizeof(buffer), "%.1f LY  (%.1f LY FULL)", view.jumpRangeLY, view.fullTankRangeLY);
        row("JUMP RANGE", buffer);

        std::snprintf(buffer, sizeof(buffer), "%.1f t  (HULL %.1f t)", view.totalMass, view.hullMass);
        row("SHIP MASS", buffer);

        std::snprintf(buffer, sizeof(buffer), "%.1f CR / t", view.pricePerTonne);
        row("PRICE", buffer, style::accent);

        y += 6.f;
        std::string note = "More fuel means more range, but a heavier ship that turns and accelerates more slowly.";

        if (view.fillQuote.tankFull)
            note = "The tank is full.";
        else if (view.fillQuote.limitedByCredits)
            note = "You can't afford a full tank; FILL TANK buys as much as your credits allow.";

        drawParagraph(target, font, note, {x, y}, page.size.x, 16, view.fillQuote.limitedByCredits ? style::warning : style::textSecondary);

        // Buttons: the main one shows what it will buy and cost.
        char fillLabel[64];

        if (view.fillQuote.tonnes > 0.f)
            std::snprintf(fillLabel, sizeof(fillLabel), "FILL  %.1f t  %.0f CR", view.fillQuote.tonnes, view.fillQuote.cost);
        else
            std::snprintf(fillLabel, sizeof(fillLabel), "%s", view.fillQuote.tankFull ? "TANK FULL" : "NO CREDITS");

        char oneLabel[48];
        std::snprintf(oneLabel, sizeof(oneLabel), "BUY %.1f t  [B]", view.oneTonneQuote.tonnes > 0.f ? view.oneTonneQuote.tonnes : 1.f);

        drawPageButton(target, font, size, 0, fillLabel, view.fillQuote.tonnes > 0.f);
        drawPageButton(target, font, size, 1, oneLabel, view.oneTonneQuote.tonnes > 0.f);
    }
};

#endif //DUSK_STATION_MENU_H
