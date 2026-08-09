#include "kaggriculture_internal.hpp"

#include "board.hpp"
#include "constants.hpp"

namespace kag {
namespace sim {

static int shed_total(const PrivateState& priv);
static void refresh_prices(GameState& gs);

static void inv_add(std::array<int, ITEM_COUNT>& inv, int item, int n = 1) {
    inv[static_cast<size_t>(item)] += n;
}

static bool inv_take(std::array<int, ITEM_COUNT>& inv, int item, int n = 1) {
    if (inv[static_cast<size_t>(item)] < n) return false;
    inv[static_cast<size_t>(item)] -= n;
    return true;
}

static std::array<int, ITEM_COUNT>& farmer_inventory(PrivateState& priv, int idx) {
    return priv.inventories[static_cast<size_t>(idx)];
}

static void apply_unit_action(GameState& gs, int player, int idx, const UnitAction& act) {
    Farm& farm = gs.farms[static_cast<size_t>(player)];
    PrivateState& priv = gs.privates[static_cast<size_t>(player)];
    const int bs = gs.config.board_size;
    const int day = gs.day;

    int fx, fy;
    if (idx == 0) {
        fx = farm.farmer_x;
        fy = farm.farmer_y;
    } else {
        const int h = idx - 1;
        if (h >= farm.hand_count) return;
        fx = farm.hands[static_cast<size_t>(h)][0];
        fy = farm.hands[static_cast<size_t>(h)][1];
    }
    auto& inv = farmer_inventory(priv, idx);

    const auto set_pos = [&](int x, int y) {
        if (idx == 0) {
            farm.farmer_x = x;
            farm.farmer_y = y;
        } else {
            const int h = idx - 1;
            farm.hands[static_cast<size_t>(h)][0] = static_cast<int16_t>(x);
            farm.hands[static_cast<size_t>(h)][1] = static_cast<int16_t>(y);
        }
    };

    switch (act.op) {
        case detail::OP_NORTH: {
            if (fy - 1 < 0) return;
            set_pos(fx, fy - 1);
            return;
        }
        case detail::OP_SOUTH: {
            if (fy + 1 >= bs) return;
            set_pos(fx, fy + 1);
            return;
        }
        case detail::OP_EAST: {
            if (fx + 1 >= bs) return;
            set_pos(fx + 1, fy);
            return;
        }
        case detail::OP_WEST: {
            if (fx - 1 < 0) return;
            set_pos(fx - 1, fy);
            return;
        }
        default:
            break;
    }

    Tile& tile = farm.tiles[static_cast<size_t>(fy)][static_cast<size_t>(fx)];

    if (act.op == detail::OP_DROP) {
        if (!detail::is_shed_adjacent(fx, fy, bs)) return;
        for (int item = 0; item < ITEM_COUNT; ++item) {
            int n = inv[static_cast<size_t>(item)];
            if (n <= 0) continue;
            int room = std::max(0, gs.config.shed_capacity - shed_total(priv));
            int take = std::min(n, room);
            if (take > 0) priv.shed[static_cast<size_t>(item)] += take;
            inv[static_cast<size_t>(item)] = 0;
        }
        return;
    }

    if (act.op == detail::OP_PICKUP) {
        if (!detail::is_shed_adjacent(fx, fy, bs)) return;
        if (act.item < 0) return;
        int n = std::max(1, act.n);
        int avail = priv.shed[static_cast<size_t>(act.item)];
        n = std::min(n, avail);
        if (n <= 0) return;
        priv.shed[static_cast<size_t>(act.item)] -= n;
        inv_add(inv, act.item, n);
        return;
    }

    if (act.op == detail::OP_PLACE) {
        if (act.item < 0 || act.item >= ITEM_COUNT) return;
        // Animal placement: standing on a matching unoccupied structure.
        bool is_animal_item = act.item >= P_COUNT;
        if (is_animal_item) {
            const int animal = act.item - P_COUNT;
            const int structure = ANIMALS[static_cast<size_t>(animal)].structure;  // 0 coop, 1 pasture
            const uint8_t need_kind = structure == 0 ? TK_COOP : TK_PASTURE;
            if (tile.kind == need_kind && !tile.has_animal()) {
                if (inv_take(inv, act.item)) {
                    tile.animal = static_cast<uint8_t>(animal);
                    tile.placed_day = static_cast<int16_t>(day);
                    tile.yield_units = 0;
                    tile.consecutive_unfed = 0;
                    tile.fed_today = 0;
                    tile.cared_today = 0;
                    tile.fertilizer_available = 0;
                    tile.pending_care_bonus = 0;
                }
                return;
            }
        }
        // Shed drop.
        if (detail::is_shed_adjacent(fx, fy, bs)) {
            int n = act.n >= 1 ? act.n : 1;
            n = std::min(n, inv[static_cast<size_t>(act.item)]);
            if (n <= 0) return;
            int room = std::max(0, gs.config.shed_capacity - shed_total(priv));
            n = std::min(n, room);
            if (n <= 0) return;
            inv[static_cast<size_t>(act.item)] -= n;
            priv.shed[static_cast<size_t>(act.item)] += n;
        }
        return;
    }

    // Tile-mutating ops require an unlocked tile.
    if (tile.kind == TK_LOCKED) return;

    if (act.op == detail::OP_PLANT) {
        if (act.crop < 0 || act.crop >= C_COUNT) return;
        if (tile.kind != TK_EMPTY) return;
        if (priv.seeds[static_cast<size_t>(act.crop)] <= 0) return;
        priv.seeds[static_cast<size_t>(act.crop)]--;
        const CropData& cd = CROPS[static_cast<size_t>(act.crop)];
        tile.kind = TK_PLANT;
        tile.crop = static_cast<uint8_t>(act.crop);
        tile.planted_day = static_cast<int16_t>(day);
        tile.watered_today = 0;
        tile.consecutive_unwatered = 1;
        tile.yield_units = static_cast<int16_t>(cd.ongoing ? 0 : 1);
        tile.max_lifespan_step = cd.ongoing ? -1
                                             : (day + cd.max_yield_day + 1) * gs.config.turns_per_day;
        tile.fertilized_until_day = -1;
        return;
    }

    if (act.op == detail::OP_WATER) {
        if (!tile.is_plant()) return;
        if (tile.watered_today) return;
        tile.watered_today = 1;
        const CropData& cd = CROPS[static_cast<size_t>(tile.crop)];
        if (!cd.ongoing) {
            const int age = day - tile.planted_day;
            const int window_start = (cd.max_yield_day + 1) / 2;
            if (window_start <= age && age <= cd.max_yield_day) {
                const int bonus = tile.fertilized_until_day >= day ? 2 : 1;
                tile.yield_units = static_cast<int16_t>(std::min<int>(cd.max_yield, tile.yield_units + bonus));
            }
        }
        return;
    }

    if (act.op == detail::OP_HARVEST) {
        if (tile.yield_units <= 0) return;
        if (tile.is_plant()) {
            const CropData& cd = CROPS[static_cast<size_t>(tile.crop)];
            if (day - tile.planted_day < cd.first_yield_day) return;
            const int units = tile.yield_units;
            tile.yield_units = 0;
            inv_add(inv, tile.crop, units);
            if (!cd.ongoing) {
                tile = Tile{};
                tile.kind = TK_EMPTY;
            }
        } else if (tile.has_animal()) {
            const int units = tile.yield_units;
            tile.yield_units = 0;
            inv_add(inv, detail::animal_product_index(tile.animal), units);
        }
        return;
    }

    if (act.op == detail::OP_FERTILIZE) {
        if (!tile.is_plant()) return;
        if (!inv_take(inv, P_FERTILIZER, 1)) return;
        tile.fertilized_until_day = static_cast<int16_t>(std::max<int>(tile.fertilized_until_day, day + 2));
        return;
    }

    if (act.op == detail::OP_DIG) {
        if (tile.kind == TK_EMPTY) return;
        if (tile.has_animal()) return;
        tile = Tile{};
        tile.kind = TK_EMPTY;
        return;
    }

    if (act.op == detail::OP_BUILD_COOP) {
        if (tile.kind != TK_EMPTY) return;
        tile = Tile{};
        tile.kind = TK_COOP;
        return;
    }

    if (act.op == detail::OP_BUILD_PASTURE) {
        if (tile.kind != TK_EMPTY) return;
        tile = Tile{};
        tile.kind = TK_PASTURE;
        return;
    }

    if (act.op == detail::OP_FEED) {
        if (!tile.has_animal()) return;
        if (tile.fed_today) return;
        if (!inv_take(inv, P_WHEAT, 1)) return;
        tile.fed_today = 1;
        return;
    }

    if (act.op == detail::OP_COLLECT_FERTILIZER) {
        if (!tile.has_animal()) return;
        if (!tile.fertilizer_available) return;
        tile.fertilizer_available = 0;
        inv_add(inv, P_FERTILIZER, 1);
        return;
    }

    if (act.op == detail::OP_CARE) {
        if (!tile.has_animal()) return;
        if (tile.cared_today) return;
        tile.cared_today = 1;
        return;
    }
}

// --- Market ---------------------------------------------------------------

static int shed_total(const PrivateState& priv) {
    int total = 0;
    for (int i = 0; i < ITEM_COUNT; ++i) total += priv.shed[static_cast<size_t>(i)];
    return total;
}

static void refresh_prices(GameState& gs) {
    for (int i = 0; i < P_COUNT; ++i)
        gs.market.prices[static_cast<size_t>(i)] =
            detail::market_price(static_cast<Product>(i), gs.market.inventory[static_cast<size_t>(i)]);
}

static void process_market(GameState& gs, const ActionInput& a0, const ActionInput& a1) {
    const ActionInput* queues[2] = {&a0, &a1};
    const int max_orders = gs.config.max_market_orders;

    // Parse each player's orders into (type, item, remaining).
    struct Order {
        int type = 0;   // 0 none, 1 SELL, 2 BUY_SEED, 3 BUY_PRODUCT, 4 BUY_ANIMAL, 5 HIRE, 6 BUY_LAND
        int item = -1;
        int remaining = 0;
        bool done = false;
    };
    Order orders[2][16];
    int counts[2] = {0, 0};
    for (int p = 0; p < 2; ++p) {
        const ActionInput& ai = *queues[p];
        int n = std::min<int>(ai.market_count, max_orders);
        for (int i = 0; i < n; ++i) {
            const MarketOrder& mo = ai.market[static_cast<size_t>(i)];
            if (mo.type == 0 || mo.n <= 0) continue;
            Order& o = orders[p][counts[p]++];
            o.type = mo.type;
            o.item = mo.item;
            o.remaining = mo.n;
        }
    }

    const int max_len = std::max(counts[0], counts[1]);

    for (int i = 0; i < max_len; ++i) {
        // Atomic HIRE / BUY_LAND handled once in player order.
        for (int p = 0; p < 2; ++p) {
            if (i < counts[p]) {
                Order& o = orders[p][static_cast<size_t>(i)];
                if (o.done) continue;
                if (o.type == 5) {  // HIRE
                    do_hire(gs, p);
                    o.done = true;
                } else if (o.type == 6) {  // BUY_LAND
                    do_buy_land(gs, p);
                    o.done = true;
                }
            }
        }

        // Per-unit lockstep for SELL / BUY_*.
        int guard = 0;
        while (guard++ < 200000) {
            // Quote both players' current unit.
            int prices[2] = {0, 0};
            int items[2] = {-1, -1};
            int types[2] = {0, 0};
            bool any = false;
            for (int p = 0; p < 2; ++p) {
                if (i < counts[p]) {
                    Order& o = orders[p][static_cast<size_t>(i)];
                    if (o.done || o.remaining <= 0) continue;
                    if (o.type == 1 && o.item >= 0 && o.item < P_COUNT) {  // SELL
                        prices[p] = detail::market_price(static_cast<Product>(o.item),
                                                         gs.market.inventory[static_cast<size_t>(o.item)]);
                        items[p] = o.item;
                        types[p] = 1;
                        any = true;
                    } else if (o.type == 3 && (o.item == P_WHEAT || o.item == P_FERTILIZER)) {  // BUY_PRODUCT
                        prices[p] = detail::market_price(static_cast<Product>(o.item),
                                                         gs.market.inventory[static_cast<size_t>(o.item)] - 1);
                        items[p] = o.item;
                        types[p] = 3;
                        any = true;
                    } else if (o.type == 2 && o.item >= 0 && o.item < C_COUNT) {  // BUY_SEED
                        prices[p] = CROPS[static_cast<size_t>(o.item)].seed_cost;
                        items[p] = o.item;
                        types[p] = 2;
                        any = true;
                    } else if (o.type == 4 && o.item >= 0 && o.item < A_COUNT) {  // BUY_ANIMAL
                        prices[p] = ANIMALS[static_cast<size_t>(o.item)].cost;
                        items[p] = o.item;
                        types[p] = 4;
                        any = true;
                    } else {
                        o.done = true;  // malformed sub-op
                    }
                }
            }
            if (!any) break;

            bool committed = false;
            for (int p = 0; p < 2; ++p) {
                if (types[p] == 0) continue;
                Order& o = orders[p][static_cast<size_t>(i)];
                Farm& farm = gs.farms[static_cast<size_t>(p)];
                PrivateState& priv = gs.privates[static_cast<size_t>(p)];
                bool ok = false;
                if (types[p] == 1) {  // SELL
                    if (priv.shed[static_cast<size_t>(items[p])] > 0) {
                        priv.shed[static_cast<size_t>(items[p])]--;
                        farm.money += prices[p];
                        if (prices[p] > PRICE_FLOOR)
                            gs.market.inventory[static_cast<size_t>(items[p])]++;
                        ok = true;
                    }
                } else if (types[p] == 3) {  // BUY_PRODUCT
                    if (farm.money >= prices[p] && shed_total(priv) < gs.config.shed_capacity) {
                        farm.money -= prices[p];
                        priv.shed[static_cast<size_t>(items[p])]++;
                        gs.market.inventory[static_cast<size_t>(items[p])]--;
                        ok = true;
                    }
                } else if (types[p] == 2) {  // BUY_SEED
                    if (farm.money >= prices[p]) {
                        farm.money -= prices[p];
                        priv.seeds[static_cast<size_t>(items[p])]++;
                        ok = true;
                    }
                } else if (types[p] == 4) {  // BUY_ANIMAL
                    if (farm.money >= prices[p] && shed_total(priv) < gs.config.shed_capacity) {
                        farm.money -= prices[p];
                        priv.shed[static_cast<size_t>(item_animal(static_cast<Animal>(items[p])))]++;
                        ok = true;
                    }
                }
                if (ok) {
                    o.remaining--;
                    committed = true;
                } else {
                    o.done = true;
                }
            }
            if (!committed) break;
        }
        refresh_prices(gs);
    }
    refresh_prices(gs);
}

// --- End of day ------------------------------------------------------------

static void decay_plants(GameState& gs, int step) {
    for (int p = 0; p < 2; ++p) {
        Farm& farm = gs.farms[static_cast<size_t>(p)];
        for (int y = 0; y < gs.config.board_size; ++y)
            for (int x = 0; x < gs.config.board_size; ++x) {
                Tile& t = farm.tiles[static_cast<size_t>(y)][static_cast<size_t>(x)];
                if (!t.is_plant()) continue;
                if (t.max_lifespan_step < 0 || step < t.max_lifespan_step) continue;
                if ((step - t.max_lifespan_step) % 2 != 0) continue;
                t.yield_units--;
                if (t.yield_units <= 0) {
                    t = Tile{};
                    t.kind = TK_WEED;
                }
            }
    }
}

static void daily_refresh_plants(GameState& gs, int day) {
    for (int p = 0; p < 2; ++p) {
        Farm& farm = gs.farms[static_cast<size_t>(p)];
        const int next_day = day + 1;
        for (int y = 0; y < gs.config.board_size; ++y)
            for (int x = 0; x < gs.config.board_size; ++x) {
                Tile& t = farm.tiles[static_cast<size_t>(y)][static_cast<size_t>(x)];
                if (!t.is_plant()) continue;
                const bool was_watered = t.watered_today != 0;
                if (was_watered)
                    t.consecutive_unwatered = 0;
                else
                    t.consecutive_unwatered++;
                t.watered_today = 0;
                if (t.consecutive_unwatered >= 2) {
                    t = Tile{};
                    t.kind = TK_WEED;
                    continue;
                }
                const CropData& cd = CROPS[static_cast<size_t>(t.crop)];
                if (cd.ongoing) {
                    const int days_since_first = next_day - t.planted_day - cd.first_yield_day;
                    if (days_since_first < 0) continue;
                    if (days_since_first % cd.interval != 0) continue;
                    const int production_count = days_since_first / cd.interval + 1;
                    if (production_count > cd.max_yield) continue;
                    const bool fertilized = was_watered && t.fertilized_until_day >= day;
                    t.yield_units = static_cast<int16_t>(
                        std::min<int>(cd.max_yield, t.yield_units + (fertilized ? 2 : 1)));
                    if (production_count == cd.max_yield)
                        t.max_lifespan_step = (next_day + 1) * gs.config.turns_per_day;
                }
            }
    }
}

static void daily_refresh_animals(GameState& gs, int day) {
    for (int p = 0; p < 2; ++p) {
        Farm& farm = gs.farms[static_cast<size_t>(p)];
        const int next_day = day + 1;
        for (int y = 0; y < gs.config.board_size; ++y)
            for (int x = 0; x < gs.config.board_size; ++x) {
                Tile& t = farm.tiles[static_cast<size_t>(y)][static_cast<size_t>(x)];
                if (!t.has_animal()) continue;
                if (t.fed_today)
                    t.consecutive_unfed = 0;
                else
                    t.consecutive_unfed++;
                if (t.consecutive_unfed >= 2) {
                    // Animal escapes; structure remains.
                    const uint8_t structure_kind = ANIMALS[static_cast<size_t>(t.animal)].structure == 0 ? TK_COOP : TK_PASTURE;
                    t = Tile{};
                    t.kind = structure_kind;
                    continue;
                }
                const AnimalData& a = ANIMALS[static_cast<size_t>(t.animal)];
                const int days_since_first = next_day - t.placed_day - a.first_yield_day;
                if (days_since_first >= 0 && days_since_first % a.interval == 0) {
                    const int bonus = t.fed_today ? t.pending_care_bonus : 0;
                    t.yield_units = static_cast<int16_t>(std::min<int>(a.max_held, t.yield_units + 1 + bonus));
                    t.pending_care_bonus = 0;
                }
                if (t.cared_today && t.fed_today) t.pending_care_bonus++;
                t.fertilizer_available = 1;
                t.fed_today = 0;
                t.cared_today = 0;
            }
    }
}

static void end_of_day(GameState& gs, int day, detail::Rng& rng) {
    for (int p = 0; p < 2; ++p) {
        Farm& farm = gs.farms[static_cast<size_t>(p)];
        PrivateState& priv = gs.privates[static_cast<size_t>(p)];
        daily_refresh_plants(gs, day);
        daily_refresh_animals(gs, day);
        // Weeds.
        for (int y = 0; y < gs.config.board_size; ++y)
            for (int x = 0; x < gs.config.board_size; ++x) {
                Tile& t = farm.tiles[static_cast<size_t>(y)][static_cast<size_t>(x)];
                if (t.kind == TK_EMPTY && rng.unit() < gs.config.weed_spawn_chance) {
                    t.kind = TK_WEED;
                }
            }
        // Drop inventories to shed.
        for (int u = 0; u <= farm.hand_count; ++u) {
            auto& inv = gs.privates[static_cast<size_t>(p)].inventories[static_cast<size_t>(u)];
            for (int item = 0; item < ITEM_COUNT; ++item) {
                int n = inv[static_cast<size_t>(item)];
                if (n <= 0) continue;
                int room = std::max(0, gs.config.shed_capacity - shed_total(priv));
                int take = std::min(n, room);
                if (take > 0) priv.shed[static_cast<size_t>(item)] += take;
                inv[static_cast<size_t>(item)] = 0;
            }
        }
        // Reset farmer, hands, hires.
        const int half = gs.config.board_size / 2;
        farm.farmer_x = half - 1;
        farm.farmer_y = half - 1;
        farm.hand_count = 0;
        farm.hires_today = 0;
        for (int u = 0; u < MAX_HANDS + 1; ++u)
            gs.privates[static_cast<size_t>(p)].inventories[static_cast<size_t>(u)].fill(0);
    }

    // Shop unlock.
    const int next_day = day + 1;
    if (next_day > 0 && gs.config.town_shop_unlock_interval > 0 &&
        next_day % gs.config.town_shop_unlock_interval == 0) {
        if (gs.town.shop_count < MAX_SHOP_INSTANCES) {
            const int idx = static_cast<int>(rng.next() % S_COUNT);
            gs.town.unlocked_shops[static_cast<size_t>(gs.town.shop_count++)] = static_cast<uint8_t>(idx);
        }
    }
}

}  // namespace sim

void apply_turn(GameState& gs, const ActionInput& a0, const ActionInput& a1, detail::Rng& rng) {
    // Player actions (simultaneous), one action per unit.
    const ActionInput* acts[2] = {&a0, &a1};
    for (int p = 0; p < 2; ++p) {
        // Atomic PLANT validation: drop all PLANT requests for a crop when the
        // requested count exceeds available seeds.
        int plant_demand[C_COUNT] = {0};
        const ActionInput& ai = *acts[p];
        if (ai.farmer.op == detail::OP_PLANT && ai.farmer.crop >= 0 && ai.farmer.crop < C_COUNT)
            plant_demand[static_cast<size_t>(ai.farmer.crop)]++;
        for (int h = 0; h < ai.hand_count; ++h)
            if (ai.hands[static_cast<size_t>(h)].op == detail::OP_PLANT && ai.hands[static_cast<size_t>(h)].crop >= 0 && ai.hands[static_cast<size_t>(h)].crop < C_COUNT)
                plant_demand[static_cast<size_t>(ai.hands[static_cast<size_t>(h)].crop)]++;
        bool blocked[C_COUNT] = {false};
        for (int c = 0; c < C_COUNT; ++c)
            if (plant_demand[static_cast<size_t>(c)] > gs.privates[static_cast<size_t>(p)].seeds[static_cast<size_t>(c)])
                blocked[static_cast<size_t>(c)] = true;

        auto allowed = [&](const UnitAction& ua) -> const UnitAction& {
            if (ua.op == detail::OP_PLANT && ua.crop >= 0 && ua.crop < C_COUNT && blocked[static_cast<size_t>(ua.crop)]) {
                static const UnitAction pass{detail::OP_PASS};
                return pass;
            }
            return ua;
        };

        sim::apply_unit_action(gs, p, 0, allowed(ai.farmer));
        for (int h = 0; h < ai.hand_count; ++h)
            sim::apply_unit_action(gs, p, h + 1, allowed(ai.hands[static_cast<size_t>(h)]));
    }

    sim::process_market(gs, a0, a1);

    // Town consumption.
    const int step = gs.step;
    if (gs.config.town_shop_sell_interval > 0 && step % gs.config.town_shop_sell_interval == 0) {
        for (int s = 0; s < gs.town.shop_count; ++s) {
            const uint16_t mask = SHOP_PRODUCTS[gs.town.unlocked_shops[static_cast<size_t>(s)]];
            const int mult = (mask & (mask - 1)) == 0 ? 2 : 1;  // single-product shop
            for (int item = 0; item < P_COUNT; ++item)
                if (mask & (1u << item))
                    gs.market.inventory[static_cast<size_t>(item)] -= mult;
        }
    }
    if (gs.config.town_center_sell_interval > 0 && step % gs.config.town_center_sell_interval == 0) {
        for (int item = 0; item < P_COUNT; ++item)
            if (item != P_FERTILIZER)
                gs.market.inventory[static_cast<size_t>(item)]--;
    }
    sim::refresh_prices(gs);

    sim::decay_plants(gs, step);

    if ((step + 1) % gs.config.turns_per_day == 0)
        sim::end_of_day(gs, gs.day, rng);

    gs.step = step + 1;
    gs.day = gs.step / gs.config.turns_per_day;
    gs.hour = gs.step % gs.config.turns_per_day;

    if (gs.step >= gs.config.episode_steps - 2) gs.done = true;
}

void do_hire(GameState& gs, int player) {
    Farm& farm = gs.farms[static_cast<size_t>(player)];
    const int cost = gs.config.farm_hand_cost_mult * detail::fib(farm.hires_today);
    if (farm.money < cost) return;
    if (farm.hand_count >= MAX_HANDS) return;
    farm.money -= cost;
    farm.hires_today++;
    // Spawn at the least-occupied shed-access tile (NWSE order).
    const int half = gs.config.board_size / 2;
    const int tiles[4][2] = {{half - 1, half - 1}, {half, half - 1}, {half - 1, half}, {half, half}};
    int best = 0, best_occ = 1 << 30;
    for (int i = 0; i < 4; ++i) {
        int occ = 0;
        if (farm.farmer_x == tiles[i][0] && farm.farmer_y == tiles[i][1]) occ++;
        for (int h = 0; h < farm.hand_count; ++h)
            if (farm.hands[static_cast<size_t>(h)][0] == tiles[i][0] && farm.hands[static_cast<size_t>(h)][1] == tiles[i][1])
                occ++;
        if (occ < best_occ) {
            best_occ = occ;
            best = i;
        }
    }
    farm.hands[static_cast<size_t>(farm.hand_count)][0] = static_cast<int16_t>(tiles[best][0]);
    farm.hands[static_cast<size_t>(farm.hand_count)][1] = static_cast<int16_t>(tiles[best][1]);
    farm.hand_count++;
}

void do_buy_land(GameState& gs, int player) {
    Farm& farm = gs.farms[static_cast<size_t>(player)];
    const int n_extra = __builtin_popcount(static_cast<unsigned>(farm.unlocked_quadrants)) - 1;
    if (n_extra >= 3) return;
    const int cost = LAND_PRICES[n_extra];
    if (farm.money < cost) return;
    farm.money -= cost;
    // Unlock order: NE(1), SW(2), SE(3).
    const int quad = n_extra + 1;  // 1=NE, 2=SW, 3=SE
    farm.unlocked_quadrants |= static_cast<uint8_t>(1u << quad);
    const int half = gs.config.board_size / 2;
    for (int y = 0; y < gs.config.board_size; ++y)
        for (int x = 0; x < gs.config.board_size; ++x)
            if (detail::quadrant_index(x, y, gs.config.board_size) == quad &&
                farm.tiles[static_cast<size_t>(y)][static_cast<size_t>(x)].kind == TK_LOCKED)
                farm.tiles[static_cast<size_t>(y)][static_cast<size_t>(x)].kind = TK_EMPTY;
}

}  // namespace kag
