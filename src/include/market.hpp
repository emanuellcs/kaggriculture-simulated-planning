#pragma once

#include "board.hpp"
#include "kaggriculture_internal.hpp"

namespace kag {

// Deterministic per-day town consumption for a product, given unlocked shops.
inline int town_demand_per_day(Product item, const TownState& town, const GameConfig& cfg) {
    int demand = 0;
    // Town center: once per center_interval turns.
    if (cfg.town_center_sell_interval > 0 && item != P_FERTILIZER) {
        demand += cfg.turns_per_day / cfg.town_center_sell_interval;
    }
    if (cfg.town_shop_sell_interval > 0) {
        const int ticks_per_day = std::max(1, cfg.turns_per_day / std::max(1, cfg.town_shop_sell_interval));
        for (int s = 0; s < town.shop_count; ++s) {
            const uint16_t mask = SHOP_PRODUCTS[town.unlocked_shops[static_cast<size_t>(s)]];
            if ((mask & (1u << item)) == 0) continue;
            // single-product shops consume 2x
            const int mult = (mask == (1u << item)) ? 2 : 1;
            demand += ticks_per_day * mult;
        }
    }
    return demand;
}

// Expected total town consumption for a product over `days` days, including
// expected future shop unlocks (shops drawn with replacement, capped at 8).
inline int expected_town_demand(Product item, const TownState& town, const GameConfig& cfg, int days) {
    int total = 0;
    for (int d = 0; d < days; ++d) {
        int extra_unlocks = 0;
        if (cfg.town_shop_unlock_interval > 0) {
            // shops unlock at end of days that are multiples of the interval.
            extra_unlocks = std::max(0, (d + 1) / cfg.town_shop_unlock_interval - town.shop_count / cfg.town_shop_unlock_interval);
        }
        total += town_demand_per_day(item, town, cfg);
        // Expected demand from future unlocks: each unlock picks a random shop;
        // expected probability a shop demands this item = count(SHOPS demanding item)/S_COUNT.
        if (extra_unlocks > 0 && town.shop_count + extra_unlocks <= MAX_SHOP_INSTANCES) {
            int demanding = 0;
            for (int s = 0; s < S_COUNT; ++s)
                if (SHOP_PRODUCTS[static_cast<size_t>(s)] & (1u << item)) demanding++;
            const double p = static_cast<double>(demanding) / static_cast<double>(S_COUNT);
            // expected extra instances in the season fraction
            total += static_cast<int>(extra_unlocks * p * std::min(1.0, static_cast<double>(town.shop_count + extra_unlocks) / MAX_SHOP_INSTANCES));
        }
    }
    return total;
}

}  // namespace kag
