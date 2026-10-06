//
// Saved games: what a save holds, where the three slot files live, and reading, writing and
// deleting them. Saves are small, human-readable text files, versioned so the format can grow.
//

#ifndef DUSK_SAVE_GAME_H
#define DUSK_SAVE_GAME_H

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

/** How many save slots the game offers. */
constexpr int saveSlotCount = 3;

/** Bumped whenever the save format changes; older files are still read (unknown keys are ignored, missing ones default). */
constexpr int saveFormatVersion = 3;

/** Longest commander name allowed, in characters. */
constexpr std::size_t maxCommanderNameLength = 16;

/** One market stock level that differs from its system's baseline: only changed entries are saved. */
struct SavedStock {
    int system = 0;
    std::string good;
    float stock = 0.f;
};

/** One trader agent (see systems/trading.h++): where it is, where it's going, and what it carries. */
struct SavedAgent {
    int system = 0;
    int destination = 0;
    double arriveTime = 0.0;
    std::string good;       // empty when the hold is empty
    int tonnes = 0;
    int visits = 0;
};

/**
 * Everything a save remembers. Saving only happens while docked, so a save doesn't need a ship
 * position: loading always puts you back in the station of `systemIndex`.
 */
struct SaveGame {
    int version = saveFormatVersion;

    std::string commanderName = "JAMES";
    double credits = 1000.0;

    /** The galaxy the commander lives in, and which system they saved in. */
    std::uint32_t galaxySeed = 1337u;
    int systemIndex = 0;

    /** The saved system's name, so the slot screen can show it without generating the galaxy. Display only. */
    std::string systemName;

    /** Fuel aboard, in tonnes. */
    float fuel = 6.f;

    /** Total time played in this slot, in seconds. */
    double playTimeSeconds = 0.0;

    /** The hold: tonnes of each good by name, and the fitted cargo bay module (0 none, 1 Mk1, 2 Mk2). */
    std::vector<std::pair<std::string, int>> cargo;
    int cargoModule = 0;

    /** The fitted fuel tank module (0 none, 1 Mk1, 2 Mk2), and whether the docking computer has been bought. */
    int fuelTank = 0;
    bool autoDock = false;

    /**
     * The trading network's state: the galaxy clock, every market stock that has moved away from
     * its baseline, and every trader agent. `hasTrade` is false for saves from before trading
     * existed, which start the network fresh.
     */
    bool hasTrade = false;
    double galaxyTime = 0.0;
    std::vector<SavedStock> stocks;
    std::vector<SavedAgent> agents;

    /** When the save was written, as readable local time ("2026-10-05 14:32"). Display only. */
    std::string savedAt;
};

/** What the main menu hands the game when it starts: which slot, what's in it, and whether it's brand new. */
struct GameLaunch {
    int slot = -1;              // 0-2, or -1 for "no slot" (nothing will be saved)
    SaveGame save;
    bool isNewGame = true;
};

/* ---- Where saves live ------------------------------------------------------------------------- */

/**
 * The folder save files go in, following each platform's convention for per-user app data:
 *
 *   macOS    ~/Library/Application Support/dusk/saves
 *   Linux    $XDG_DATA_HOME/dusk/saves, or ~/.local/share/dusk/saves
 *   Windows  %APPDATA%\dusk\saves
 *
 * Setting the DUSK_SAVE_DIR environment variable overrides all of these (handy for testing).
 * If no home folder can be found, saves fall back to a "saves" folder in the working directory.
 */
inline std::filesystem::path saveDirectory()
{
    namespace fs = std::filesystem;

    if (const char* overrideDir = std::getenv("DUSK_SAVE_DIR"); overrideDir && *overrideDir)
        return fs::path(overrideDir);

#if defined(_WIN32)
    if (const char* appData = std::getenv("APPDATA"); appData && *appData)
        return fs::path(appData) / "dusk" / "saves";
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME"); home && *home)
        return fs::path(home) / "Library" / "Application Support" / "dusk" / "saves";
#else
    if (const char* dataHome = std::getenv("XDG_DATA_HOME"); dataHome && *dataHome)
        return fs::path(dataHome) / "dusk" / "saves";

    if (const char* home = std::getenv("HOME"); home && *home)
        return fs::path(home) / ".local" / "share" / "dusk" / "saves";
#endif

    return fs::path("saves");
}

/** The file for slot `slot` (0-based): slot1.sav, slot2.sav, slot3.sav. */
inline std::filesystem::path saveSlotPath(int slot)
{
    return saveDirectory() / ("slot" + std::to_string(slot + 1) + ".sav");
}

/* ---- Small helpers ---------------------------------------------------------------------------- */

/** Local time now, formatted "YYYY-MM-DD HH:MM". */
inline std::string currentTimestamp()
{
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};

#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif

    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &local);
    return buffer;
}

/** Play time as "2h 05m" or "12m". */
inline std::string formatPlayTime(double seconds)
{
    const long minutes = static_cast<long>(std::max(0.0, seconds) / 60.0);
    char buffer[32];

    if (minutes >= 60)
        std::snprintf(buffer, sizeof(buffer), "%ldh %02ldm", minutes / 60, minutes % 60);
    else
        std::snprintf(buffer, sizeof(buffer), "%ldm", minutes);

    return buffer;
}

/** True for characters allowed in a commander name: letters, digits, space, hyphen, apostrophe, full stop. */
inline bool isCommanderNameCharacter(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '-' || c == '\'' || c == '.';
}

/**
 * Cleans a commander name: uppercase (Elite style), allowed characters only, single spaces, no
 * leading or trailing spaces, at most maxCommanderNameLength long. An empty result becomes JAMES.
 */
inline std::string sanitiseCommanderName(const std::string& raw)
{
    std::string name;

    for (const char c : raw)
    {
        if (!isCommanderNameCharacter(c))
            continue;

        if (c == ' ' && (name.empty() || name.back() == ' '))
            continue;

        name += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        if (name.size() >= maxCommanderNameLength)
            break;
    }

    while (!name.empty() && name.back() == ' ')
        name.pop_back();

    return name.empty() ? std::string("JAMES") : name;
}

/** A fresh game for a newly named commander: system 0, a full tank and 1,000 credits. */
inline SaveGame newSaveGame(const std::string& commanderName, std::uint32_t galaxySeed)
{
    SaveGame save;
    save.commanderName = sanitiseCommanderName(commanderName);
    save.galaxySeed = galaxySeed;
    return save;
}

/* ---- Reading and writing ---------------------------------------------------------------------- */

/** Serialises a save as "key=value" lines under a "dusk-save <version>" header. */
inline std::string serialiseSave(const SaveGame& save)
{
    std::ostringstream out;
    out.precision(10);
    out << "dusk-save " << saveFormatVersion << '\n';
    out << "name=" << save.commanderName << '\n';
    out << "credits=" << save.credits << '\n';
    out << "galaxySeed=" << save.galaxySeed << '\n';
    out << "system=" << save.systemIndex << '\n';
    out << "systemName=" << save.systemName << '\n';
    out << "fuel=" << save.fuel << '\n';
    out << "playTime=" << save.playTimeSeconds << '\n';
    out << "cargoModule=" << save.cargoModule << '\n';
    out << "fuelTank=" << save.fuelTank << '\n';
    out << "autoDock=" << (save.autoDock ? 1 : 0) << '\n';

    for (const auto& [good, tonnes] : save.cargo)
        out << "cargo=" << good << ',' << tonnes << '\n';

    if (save.hasTrade)
    {
        out << "galaxyTime=" << save.galaxyTime << '\n';

        // Tenths of a tonne and of a second are plenty, and keep these (many) lines short.
        char line[160];

        for (const SavedStock& stock : save.stocks)
        {
            std::snprintf(line, sizeof(line), "stock=%d,%s,%.1f\n", stock.system, stock.good.c_str(), static_cast<double>(stock.stock));
            out << line;
        }

        for (const SavedAgent& agent : save.agents)
        {
            std::snprintf(line, sizeof(line), "agent=%d,%d,%.1f,%s,%d,%d\n", agent.system, agent.destination, agent.arriveTime,
                          agent.good.c_str(), agent.tonnes, agent.visits);
            out << line;
        }
    }

    out << "savedAt=" << save.savedAt << '\n';
    return out.str();
}

/** Splits "a,b,c" on commas (goods names contain spaces but never commas). */
inline std::vector<std::string> splitFields(const std::string& text)
{
    std::vector<std::string> fields;
    std::string field;
    std::istringstream in(text);

    while (std::getline(in, field, ','))
        fields.push_back(field);

    // getline drops a trailing empty field ("a,b,"); keep it so empty goods names round-trip.
    if (!text.empty() && text.back() == ',')
        fields.emplace_back();

    return fields;
}

/**
 * Parses a save. Returns nothing if the header is missing (not a save file). Unknown keys are
 * skipped and missing ones keep their defaults, so files from older or newer versions still load.
 * Values are sanity-checked: names are re-sanitised, and credits, fuel and system can't go negative.
 */
inline std::optional<SaveGame> parseSave(const std::string& text)
{
    std::istringstream in(text);
    std::string line;

    if (!std::getline(in, line) || line.rfind("dusk-save ", 0) != 0)
        return std::nullopt;

    SaveGame save;
    save.version = std::atoi(line.c_str() + 10);

    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        const std::size_t equals = line.find('=');

        if (equals == std::string::npos)
            continue;

        const std::string key = line.substr(0, equals);
        const std::string value = line.substr(equals + 1);

        if (key == "name")
            save.commanderName = sanitiseCommanderName(value);
        else if (key == "credits")
            save.credits = std::max(0.0, std::atof(value.c_str()));
        else if (key == "galaxySeed")
            save.galaxySeed = static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        else if (key == "system")
            save.systemIndex = std::max(0, std::atoi(value.c_str()));
        else if (key == "systemName")
            save.systemName = value;
        else if (key == "fuel")
            save.fuel = std::max(0.f, static_cast<float>(std::atof(value.c_str())));
        else if (key == "playTime")
            save.playTimeSeconds = std::max(0.0, std::atof(value.c_str()));
        else if (key == "cargoModule")
            save.cargoModule = std::clamp(std::atoi(value.c_str()), 0, 2);
        else if (key == "fuelTank")
            save.fuelTank = std::clamp(std::atoi(value.c_str()), 0, 2);
        else if (key == "autoDock")
            save.autoDock = std::atoi(value.c_str()) != 0;
        else if (key == "cargo")
        {
            const auto fields = splitFields(value);

            if (fields.size() == 2)
                save.cargo.emplace_back(fields[0], std::max(0, std::atoi(fields[1].c_str())));
        }
        else if (key == "galaxyTime")
        {
            save.hasTrade = true;
            save.galaxyTime = std::max(0.0, std::atof(value.c_str()));
        }
        else if (key == "stock")
        {
            const auto fields = splitFields(value);

            if (fields.size() == 3)
                save.stocks.push_back({std::max(0, std::atoi(fields[0].c_str())), fields[1], std::max(0.f, static_cast<float>(std::atof(fields[2].c_str())))});
        }
        else if (key == "agent")
        {
            const auto fields = splitFields(value);

            if (fields.size() == 6)
            {
                save.agents.push_back({
                    std::max(0, std::atoi(fields[0].c_str())),
                    std::max(0, std::atoi(fields[1].c_str())),
                    std::max(0.0, std::atof(fields[2].c_str())),
                    fields[3],
                    std::max(0, std::atoi(fields[4].c_str())),
                    std::max(0, std::atoi(fields[5].c_str()))
                });
            }
        }
        else if (key == "savedAt")
            save.savedAt = value;
    }

    return save;
}

/** Reads slot `slot`, or returns nothing if it's empty or unreadable. */
inline std::optional<SaveGame> readSave(int slot)
{
    std::ifstream file(saveSlotPath(slot), std::ios::binary);

    if (!file)
        return std::nullopt;

    std::ostringstream text;
    text << file.rdbuf();
    return parseSave(text.str());
}

/**
 * Writes a save to slot `slot`, stamping it with the current time. The file is written to a
 * temporary name first and then renamed over the old one, so a crash mid-write can never leave
 * a half-written slot. Returns false (with a reason in `error`, if given) on failure.
 */
inline bool writeSave(int slot, SaveGame save, std::string* error = nullptr)
{
    namespace fs = std::filesystem;

    if (slot < 0 || slot >= saveSlotCount)
    {
        if (error)
            *error = "no save slot";
        return false;
    }

    std::error_code ec;
    fs::create_directories(saveDirectory(), ec);

    save.savedAt = currentTimestamp();
    const fs::path finalPath = saveSlotPath(slot);
    const fs::path tempPath = finalPath.string() + ".tmp";

    {
        std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);

        if (!file)
        {
            if (error)
                *error = "can't write " + tempPath.string();
            return false;
        }

        file << serialiseSave(save);

        if (!file)
        {
            if (error)
                *error = "write failed";
            return false;
        }
    }

    fs::rename(tempPath, finalPath, ec);

    if (ec)
    {
        // Some filesystems refuse to rename over an existing file; replace it in two steps.
        fs::remove(finalPath, ec);
        fs::rename(tempPath, finalPath, ec);
    }

    if (ec && error)
        *error = ec.message();

    return !ec;
}

/** Deletes slot `slot`'s file. Returns true if the slot is empty afterwards. */
inline bool deleteSave(int slot)
{
    std::error_code ec;
    std::filesystem::remove(saveSlotPath(slot), ec);
    return !std::filesystem::exists(saveSlotPath(slot), ec);
}

#endif //DUSK_SAVE_GAME_H
