//
// Trading: every system's market (stock, prices, supply and demand), the quotes the player buys
// and sells at, and the trader agents that move goods between systems on their own.
//
// This file is pure logic with no drawing, so the whole economy can be tested headless.
//
//   Markets   each system keeps a stock of every good. Stock sets the price: plentiful goods are
//             cheap, scarce ones dear. Stock drifts back to its baseline over time.
//   Prices    a system's exports are cheap, everything else costs a little more than average,
//             poor systems charge more than progressive ones, and stock moves the price.
//   Agents    a few hundred simple-reflex traders hop between nearby systems, buying where goods
//             are cheap and selling where they're dear. Their trades move stock, so prices
//             respond to them (and to you) rather than staying fixed.
//

#ifndef DUSK_TRADING_H
#define DUSK_TRADING_H

#include "objects/cargo.h++"
#include "procgen/galaxy.h++"
#include "systems/economy.h++"
#include "systems/save_game.h++"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace trading {

    /* ---- Tuning ------------------------------------------------------------------------------ */

    /** Seconds for a market's stock to close about 63% of its gap to baseline. */
    constexpr double recoverySeconds = 600.0;

    /** A system's exports cost this fraction of the galaxy base price (before tier and stock). */
    constexpr float exportFactor = 0.85f;

    /** Goods a system doesn't export cost between these multiples of the base price, varying by system and good. */
    constexpr float importFactorMin = 1.02f;
    constexpr float importFactorSpread = 0.18f;

    /**
     * How much of a system's economy tier carries into goods prices. Fuel uses the full tier
     * multiplier (poor systems charge 15% more); goods use only this share of it, because goods
     * are traded between systems of every tier and the full effect would swamp the real
     * differences between exporters and importers.
     */
    constexpr float tierInfluence = 0.4f;

    /** Baseline stock in tonnes: exporters hold a lot, everyone else a modest amount. */
    constexpr float exportStockMin = 120.f;
    constexpr float exportStockSpread = 80.f;
    constexpr float importStockMin = 80.f;
    constexpr float importStockSpread = 60.f;

    /** How strongly stock moves the price: price scales with (baseline / stock) to this power. */
    constexpr float stockPriceExponent = 0.25f;
    constexpr float minStockMultiplier = 0.55f;
    constexpr float maxStockMultiplier = 1.8f;

    /** Dealers buy this fraction below the mid price and sell this fraction above it. */
    constexpr float tradeSpread = 0.02f;

    /* ---- Trader agents ----------------------------------------------------------------------- */

    constexpr int agentCount = 300;

    /**
     * How many tonnes an agent's hold takes. Traders are bulk freighters: with much less than
     * this their trades barely move a market (the whole point of having them), with much more they
     * would swamp the player's own effect on prices.
     */
    constexpr int agentHoldTonnes = 50;

    /** Agents only hop this far, in light years, and only to one of this many nearest systems. */
    constexpr float agentHopRange = 40.f;
    constexpr int agentNeighbourCount = 8;

    /** Game seconds a hop takes per light year, and how long an agent lingers in port. */
    constexpr double secondsPerLightYear = 4.5;
    constexpr double minDwellSeconds = 10.0;
    constexpr double maxDwellSeconds = 25.0;

    /**
     * The condition-action thresholds, as multiples of a good's galaxy base price. An agent buys
     * only what costs at most buyRatio here, and sells only where its cargo fetches at least sellRatio.
     */
    constexpr float agentBuyRatio = 0.90f;
    constexpr float agentSellRatio = 1.02f;

    /** An agent never drains a market below this fraction of its baseline stock. */
    constexpr float agentReserveFraction = 0.25f;

    /** Game seconds a brand-new galaxy's traders are run before you start, so markets begin with some history. */
    constexpr double warmUpSeconds = 1800.0;

    /** Game seconds a hyperspace jump adds to the clock, per light year travelled. */
    constexpr double jumpSecondsPerLightYear = 6.0;

    /* ---- Small helpers ----------------------------------------------------------------------- */

    /** A stable pseudo-random value in [0, 1) from three integers (no random state to keep). */
    inline float hash01(std::uint32_t a, std::uint32_t b, std::uint32_t c)
    {
        std::uint32_t h = a * 0x9E3779B1U + b * 0x85EBCA77U + c * 0xC2B2AE3DU;
        h ^= h >> 15;
        h *= 0x2C1B3C6DU;
        h ^= h >> 12;
        h *= 0x297A2D39U;
        h ^= h >> 15;
        return static_cast<float>(h & 0xFFFFFFU) / 16777216.f;
    }

    /** Why a quote delivered less than was asked for. */
    enum class Limit { None, SoldOut, HoldFull, NoCredits, NoCargo };

    /** What a purchase or sale would deliver, and what it costs or pays. */
    struct TradeQuote {
        int tonnes = 0;
        double value = 0.0;
        Limit limit = Limit::None;
    };

    /** What a trader agent last did at a system, shown on the market page ("TRADER LOADED 9 t WINES"). */
    enum class TraderEvent { None, Loaded, Sold };

    /** One trader agent: a simple-reflex machine. It remembers nothing but where it is and what it carries. */
    struct Agent {
        int system = 0;
        int destination = 0;
        double arriveTime = 0.0;
        int good = -1;       // -1 when the hold is empty
        int tonnes = 0;
        int visits = 0;      // only used to vary tie-breaks between otherwise identical choices
    };

} // namespace trading

/**
 * The whole trading economy: a market for every system and the agents moving goods between
 * them. Markets update lazily (a market is brought up to date only when someone looks at it or
 * trades in it), so a thousand systems cost almost nothing to keep.
 */
class TradeNetwork {
public:
    /** One system's market. Stock is in tonnes; everything else is derived from it. */
    struct Market {
        std::array<float, goodCount> stock{};
        double lastUpdate = 0.0;

        /** The last thing an agent did here, for the market page. */
        trading::TraderEvent lastEvent = trading::TraderEvent::None;
        int lastEventGood = -1;
        int lastEventTonnes = 0;
        double lastEventTime = 0.0;
    };

    /** Builds markets and agents for a galaxy and runs the traders for a while, so prices have history. */
    void initialise(const Galaxy& galaxy, double warmUp = trading::warmUpSeconds)
    {
        build(galaxy);
        placeAgents();
        advance(warmUp);
    }

    /**
     * Rebuilds the network from a save: market stocks that had moved, the agents, and the clock.
     * Saves from before trading existed (no network data) start a fresh network instead.
     */
    void restore(const Galaxy& galaxy, const SaveGame& save)
    {
        if (!save.hasTrade)
        {
            initialise(galaxy);
            return;
        }

        build(galaxy);
        now_ = save.galaxyTime;

        for (Market& market : markets_)
            market.lastUpdate = now_;

        for (const SavedStock& saved : save.stocks)
        {
            const int good = goodIndex(saved.good);

            if (saved.system >= 0 && saved.system < systemCount() && good >= 0)
                markets_[static_cast<std::size_t>(saved.system)].stock[static_cast<std::size_t>(good)] = saved.stock;
        }

        for (const SavedAgent& saved : save.agents)
        {
            if (saved.system >= systemCount() || saved.destination >= systemCount())
                continue;

            trading::Agent agent;
            agent.system = saved.system;
            agent.destination = saved.destination;
            agent.arriveTime = saved.arriveTime;
            agent.good = saved.good.empty() ? -1 : goodIndex(saved.good);
            agent.tonnes = agent.good >= 0 ? saved.tonnes : 0;
            agent.visits = saved.visits;
            agents_.push_back(agent);
        }

        if (agents_.empty())
            placeAgents();

        // Statistics bookkeeping parallel to the agents (placeAgents() sizes it itself).
        agentCostBasis_.assign(agents_.size(), 0.f);
    }

    /** Writes the network into a save: the clock, every stock that has moved from baseline, every agent. */
    void fillSave(SaveGame& save) const
    {
        save.hasTrade = true;
        save.galaxyTime = now_;
        save.stocks.clear();
        save.agents.clear();

        for (int system = 0; system < systemCount(); ++system)
        {
            const Market& market = marketAt(system);

            for (int good = 0; good < goodCount; ++good)
            {
                const float stock = market.stock[static_cast<std::size_t>(good)];

                // Stocks within a few percent of baseline (under about 1% on the price) aren't worth saving.
                const float base = baseline(system, good);

                if (std::abs(stock - base) > std::max(2.f, 0.04f * base))
                    save.stocks.push_back({system, goodName(good), stock});
            }
        }

        for (const trading::Agent& agent : agents_)
            save.agents.push_back({agent.system, agent.destination, agent.arriveTime, goodName(agent.good), agent.tonnes, agent.visits});
    }

    /** The galaxy clock, in game seconds. */
    double time() const
    {
        return now_;
    }

    /** Runs the clock forward to `toTime`, letting every agent whose trip has ended act. Time never goes backwards. */
    void advance(double toTime)
    {
        constexpr double step = 5.0;

        while (now_ < toTime)
        {
            const double end = std::min(toTime, now_ + step);

            for (std::size_t index = 0; index < agents_.size(); ++index)
            {
                trading::Agent& agent = agents_[index];

                while (agent.arriveTime <= end)
                    agentArrives(index, agent.arriveTime);
            }

            now_ = end;
        }
    }

    /* ---- Prices ------------------------------------------------------------------------------ */

    int systemCount() const
    {
        return galaxy_ ? static_cast<int>(galaxy_->systems.size()) : 0;
    }

    /** True if `system` lists `good` among its exports. */
    bool exports(int system, int good) const
    {
        return (exportMask_[static_cast<std::size_t>(system)] >> good) & 1u;
    }

    /** The galaxy-wide reference price of a good: the yardstick "cheap" and "dear" are measured against. */
    float referencePrice(int good) const
    {
        return static_cast<float>(basePriceFor(goodName(good)));
    }

    /** Stock the system's market settles back to when nobody trades in it, in tonnes. */
    float baseline(int system, int good) const
    {
        const float roll = trading::hash01(static_cast<std::uint32_t>(system), static_cast<std::uint32_t>(good), 11u);

        return exports(system, good)
            ? trading::exportStockMin + trading::exportStockSpread * roll
            : trading::importStockMin + trading::importStockSpread * roll;
    }

    /** Current stock of a good in a system's market, in tonnes (brought up to date first). */
    float stock(int system, int good) const
    {
        return marketAt(system).stock[static_cast<std::size_t>(good)];
    }

    /** What a good costs at `system` with `stockLevel` tonnes on hand, before the dealer's spread. */
    float midPriceAt(int system, int good, float stockLevel) const
    {
        const SystemInfo& info = galaxy_->systems[static_cast<std::size_t>(system)];

        // Exports are cheap at home; everything else carries a local demand premium that varies
        // by system and good, so no two systems price the same goods alike.
        const float local = exports(system, good)
            ? trading::exportFactor
            : trading::importFactorMin + trading::importFactorSpread * trading::hash01(static_cast<std::uint32_t>(system), static_cast<std::uint32_t>(good), 23u);

        // Plentiful stock is cheap, scarce stock dear. Stock at baseline has no effect.
        const float base = baseline(system, good);
        const float ratio = base / std::max(stockLevel, 0.25f * base);
        const float stockMultiplier = std::clamp(std::pow(ratio, trading::stockPriceExponent), trading::minStockMultiplier, trading::maxStockMultiplier);

        const float tier = 1.f + (economyTierMultiplier(info.economyTier) - 1.f) * trading::tierInfluence;

        return referencePrice(good) * tier * local * stockMultiplier;
    }

    /** Current mid price of a good in a system. */
    float midPrice(int system, int good) const
    {
        return midPriceAt(system, good, stock(system, good));
    }

    /** What the player pays for the next tonne, and is paid for the next tonne. */
    float buyPrice(int system, int good) const
    {
        return midPrice(system, good) * (1.f + trading::tradeSpread);
    }

    /** What the player is paid for the next tonne: the mid price less the dealer's spread. */
    float sellPrice(int system, int good) const
    {
        return midPrice(system, good) * (1.f - trading::tradeSpread);
    }

    /** Mid price relative to the galaxy reference: -0.25 means 25% cheaper than average, +0.2 means 20% dearer. */
    float versusAverage(int system, int good) const
    {
        return midPrice(system, good) / referencePrice(good) - 1.f;
    }

    /* ---- The player's trades ----------------------------------------------------------------- */

    /**
     * Quotes buying up to `wanted` tonnes: one tonne at a time, with the price rising as stock
     * falls, stopping when the hold (`room`), the stock or the money (`credits`) runs out.
     */
    trading::TradeQuote quoteBuy(int system, int good, int wanted, int room, double credits) const
    {
        trading::TradeQuote quote;
        float level = stock(system, good);

        for (int tonne = 0; tonne < wanted; ++tonne)
        {
            if (room - tonne <= 0) { quote.limit = trading::Limit::HoldFull; break; }
            if (level < 1.f) { quote.limit = trading::Limit::SoldOut; break; }

            const double price = static_cast<double>(midPriceAt(system, good, level)) * (1.0 + trading::tradeSpread);

            if (credits < quote.value + price) { quote.limit = trading::Limit::NoCredits; break; }

            quote.value += price;
            ++quote.tonnes;
            level -= 1.f;
        }

        return quote;
    }

    /** Quotes selling up to `wanted` tonnes of a good of which `held` tonnes are aboard, the price falling as stock builds. */
    trading::TradeQuote quoteSell(int system, int good, int wanted, int held) const
    {
        trading::TradeQuote quote;
        float level = stock(system, good);

        for (int tonne = 0; tonne < wanted; ++tonne)
        {
            if (held - tonne <= 0) { quote.limit = trading::Limit::NoCargo; break; }

            quote.value += static_cast<double>(midPriceAt(system, good, level)) * (1.0 - trading::tradeSpread);
            ++quote.tonnes;
            level += 1.f;
        }

        return quote;
    }

    /** Carries out a purchase as quoted: the market's stock falls by what was bought. */
    trading::TradeQuote buy(int system, int good, int wanted, int room, double credits)
    {
        const trading::TradeQuote quote = quoteBuy(system, good, wanted, room, credits);
        marketAt(system).stock[static_cast<std::size_t>(good)] -= static_cast<float>(quote.tonnes);
        return quote;
    }

    /** Carries out a sale as quoted: the market's stock rises by what was sold. */
    trading::TradeQuote sell(int system, int good, int wanted, int held)
    {
        const trading::TradeQuote quote = quoteSell(system, good, wanted, held);
        marketAt(system).stock[static_cast<std::size_t>(good)] += static_cast<float>(quote.tonnes);
        return quote;
    }

    /* ---- Reading the market for the UI ------------------------------------------------------- */

    /** Seconds since an agent last traded at `system`, or a negative number if none has. */
    double secondsSinceTraderEvent(int system) const
    {
        const Market& market = marketAt(system);
        return market.lastEvent == trading::TraderEvent::None ? -1.0 : now_ - market.lastEventTime;
    }

    /** "TRADER LOADED 9 t WINES  (40 s ago)", or an empty string if no trader has called here yet. */
    std::string describeTraderEvent(int system) const
    {
        const Market& market = marketAt(system);

        if (market.lastEvent == trading::TraderEvent::None)
            return {};

        // Goods are shown in capitals, like everything else on the station screen.
        std::string good = goodName(market.lastEventGood);

        for (char& c : good)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        const long ago = static_cast<long>(std::max(0.0, now_ - market.lastEventTime));
        char buffer[96];
        std::snprintf(buffer, sizeof(buffer), "TRADER %s %d t %s  (%ld %s ago)",
                      market.lastEvent == trading::TraderEvent::Loaded ? "LOADED" : "SOLD",
                      market.lastEventTonnes,
                      good.c_str(),
                      ago >= 120 ? ago / 60 : ago,
                      ago >= 120 ? "min" : "s");
        return buffer;
    }

    /** The agents, for tests and tools. */
    const std::vector<trading::Agent>& agents() const
    {
        return agents_;
    }

    /** Running totals of what the agents have done since the network was built: tonnes loaded and sold, and the trips they took. */
    struct AgentStats {
        long tonnesLoaded = 0;
        long tonnesSold = 0;
        long arrivals = 0;
        long arrivalsThatTraded = 0;
        double marginSum = 0.0;    // sum over completed deliveries of (sale price - purchase price) per tonne, as a fraction of the purchase price
        long deliveries = 0;
    };

    /** The agents' running totals, for tests and tools. */
    const AgentStats& stats() const
    {
        return stats_;
    }

private:
    const Galaxy* galaxy_ = nullptr;
    double now_ = 0.0;

    mutable std::vector<Market> markets_;
    std::vector<std::uint16_t> exportMask_;
    std::vector<std::vector<int>> neighbours_;
    std::vector<trading::Agent> agents_;
    std::vector<float> agentCostBasis_;   // statistics only: price paid per tonne, by agent; never read by the rules
    AgentStats stats_;

    /** Sets up markets at baseline, the export table, and each system's nearest neighbours. */
    void build(const Galaxy& galaxy)
    {
        galaxy_ = &galaxy;
        now_ = 0.0;
        agents_.clear();
        agentCostBasis_.clear();
        stats_ = {};

        const std::size_t count = galaxy.systems.size();
        exportMask_.assign(count, 0);

        for (std::size_t system = 0; system < count; ++system)
        {
            for (const std::string& name : galaxy.systems[system].goods)
            {
                const int good = goodIndex(name);

                if (good >= 0)
                    exportMask_[system] = static_cast<std::uint16_t>(exportMask_[system] | (1u << good));
            }
        }

        markets_.assign(count, Market{});

        for (std::size_t system = 0; system < count; ++system)
        {
            for (int good = 0; good < goodCount; ++good)
                markets_[system].stock[static_cast<std::size_t>(good)] = baseline(static_cast<int>(system), good);
        }

        // Each system's nearest neighbours within hop range, nearest first: where its traders can go.
        neighbours_.assign(count, {});

        for (std::size_t from = 0; from < count; ++from)
        {
            std::vector<std::pair<float, int>> near;

            for (std::size_t to = 0; to < count; ++to)
            {
                if (to == from)
                    continue;

                const float distance = galacticDistance(galaxy.systems[from], galaxy.systems[to]);

                if (distance <= trading::agentHopRange)
                    near.emplace_back(distance, static_cast<int>(to));
            }

            std::sort(near.begin(), near.end());

            for (std::size_t i = 0; i < near.size() && i < static_cast<std::size_t>(trading::agentNeighbourCount); ++i)
                neighbours_[from].push_back(near[i].second);
        }
    }

    /** Scatters the agents across the galaxy with empty holds, each about to "arrive" where it stands. */
    void placeAgents()
    {
        std::mt19937 rng(galaxy_->seed ^ 0xA6E47u);
        std::uniform_int_distribution<int> systemDist(0, systemCount() - 1);
        std::uniform_real_distribution<double> startDist(0.0, 60.0);

        agents_.assign(static_cast<std::size_t>(trading::agentCount), trading::Agent{});
        agentCostBasis_.assign(agents_.size(), 0.f);

        for (trading::Agent& agent : agents_)
        {
            agent.system = systemDist(rng);
            agent.destination = agent.system;
            agent.arriveTime = now_ + startDist(rng);
        }
    }

    /** Brings a market up to time `t`: each stock closes part of its gap to baseline. Never runs backwards. */
    Market& marketAt(int system, double t) const
    {
        Market& market = markets_[static_cast<std::size_t>(system)];

        if (t > market.lastUpdate)
        {
            const float keep = static_cast<float>(std::exp(-(t - market.lastUpdate) / trading::recoverySeconds));

            for (int good = 0; good < goodCount; ++good)
            {
                const float base = baseline(system, good);
                float& level = market.stock[static_cast<std::size_t>(good)];
                level = base + (level - base) * keep;
            }

            market.lastUpdate = t;
        }

        return market;
    }

    Market& marketAt(int system) const
    {
        return marketAt(system, now_);
    }

    /**
     * An agent's trip has ended; it acts on what it can see here, using only these rules:
     *
     *   1. Holding cargo, and it fetches at least sellRatio of its reference price here: sell it all.
     *   2. Hold empty, and some good costs at most buyRatio of its reference price here: load the
     *      cheapest, as much as the hold takes without draining the market.
     *   3. Then move on: with cargo, to the nearest system that doesn't export it (where it will be
     *      dear); with an empty hold, to one of the nearest few systems.
     *
     * No memory of prices elsewhere, no planning, no learning: just the percept and the rules.
     */
    void agentArrives(std::size_t index, double t)
    {
        trading::Agent& agent = agents_[index];
        agent.system = agent.destination;
        agent.visits += 1;

        Market& market = marketAt(agent.system, t);
        const auto ratioAt = [&](int good)
        {
            return midPriceAt(agent.system, good, market.stock[static_cast<std::size_t>(good)]) / referencePrice(good);
        };

        ++stats_.arrivals;
        bool traded = false;

        // Rule 1: sell where the cargo is dear.
        if (agent.good >= 0 && ratioAt(agent.good) >= trading::agentSellRatio)
        {
            const float price = midPriceAt(agent.system, agent.good, market.stock[static_cast<std::size_t>(agent.good)]);
            market.stock[static_cast<std::size_t>(agent.good)] += static_cast<float>(agent.tonnes);
            record(market, trading::TraderEvent::Sold, agent.good, agent.tonnes, t);

            stats_.tonnesSold += agent.tonnes;
            ++stats_.deliveries;

            if (index < agentCostBasis_.size() && agentCostBasis_[index] > 0.f)
                stats_.marginSum += static_cast<double>(price / agentCostBasis_[index] - 1.f);

            agent.good = -1;
            agent.tonnes = 0;
            traded = true;
        }

        // Rule 2: load what is cheap.
        if (agent.good < 0)
        {
            int cheapest = -1;
            float cheapestRatio = trading::agentBuyRatio;

            for (int good = 0; good < goodCount; ++good)
            {
                const float ratio = ratioAt(good);

                if (ratio <= cheapestRatio)
                {
                    cheapest = good;
                    cheapestRatio = ratio;
                }
            }

            if (cheapest >= 0)
            {
                const float reserve = trading::agentReserveFraction * baseline(agent.system, cheapest);
                const int available = static_cast<int>(std::floor(market.stock[static_cast<std::size_t>(cheapest)] - reserve));
                const int take = std::min(trading::agentHoldTonnes, available);

                if (take >= 3)
                {
                    if (index < agentCostBasis_.size())
                        agentCostBasis_[index] = midPriceAt(agent.system, cheapest, market.stock[static_cast<std::size_t>(cheapest)]);

                    market.stock[static_cast<std::size_t>(cheapest)] -= static_cast<float>(take);
                    agent.good = cheapest;
                    agent.tonnes = take;
                    record(market, trading::TraderEvent::Loaded, cheapest, take, t);

                    stats_.tonnesLoaded += take;
                    traded = true;
                }
            }
        }

        if (traded)
            ++stats_.arrivalsThatTraded;

        // Rule 3: move on.
        const std::vector<int>& near = neighbours_[static_cast<std::size_t>(agent.system)];
        const std::uint32_t tieBreak = static_cast<std::uint32_t>(index) * 7919u + static_cast<std::uint32_t>(agent.visits);

        if (near.empty())
        {
            agent.destination = agent.system;
            agent.arriveTime = t + 60.0;
            return;
        }

        std::vector<int> candidates;

        if (agent.good >= 0)
        {
            for (const int system : near)
            {
                if (!exports(system, agent.good))
                    candidates.push_back(system);

                if (candidates.size() >= 4)
                    break;
            }
        }
        else
        {
            for (std::size_t i = 0; i < near.size() && i < 3; ++i)
                candidates.push_back(near[i]);
        }

        if (candidates.empty())
            candidates.push_back(near.front());

        const int chosen = candidates[static_cast<std::size_t>(trading::hash01(tieBreak, 5u, 9u) * static_cast<float>(candidates.size())) % candidates.size()];
        const float distance = galacticDistance(galaxy_->systems[static_cast<std::size_t>(agent.system)], galaxy_->systems[static_cast<std::size_t>(chosen)]);
        const double dwell = trading::minDwellSeconds + (trading::maxDwellSeconds - trading::minDwellSeconds) * static_cast<double>(trading::hash01(tieBreak, 7u, 13u));

        agent.destination = chosen;
        agent.arriveTime = t + dwell + static_cast<double>(distance) * trading::secondsPerLightYear;
    }

    /** Notes what an agent just did at a market, for the market page. */
    static void record(Market& market, trading::TraderEvent event, int good, int tonnes, double t)
    {
        market.lastEvent = event;
        market.lastEventGood = good;
        market.lastEventTonnes = tonnes;
        market.lastEventTime = t;
    }
};

#endif //DUSK_TRADING_H
