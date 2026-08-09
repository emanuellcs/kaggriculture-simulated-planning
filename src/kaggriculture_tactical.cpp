#include "kaggriculture_internal.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <queue>

#include "board.hpp"
#include "constants.hpp"
#include "engine.hpp"
#include "market.hpp"
#include "plan.hpp"
#include "tactical.hpp"

namespace kag {
namespace {

constexpr int MAX_TASKS = 256;

struct Task {
    int x = -1, y = -1;
    uint8_t op = detail::OP_PASS;
    int8_t arg = -1;
    int priority = 0;
    bool one_shot = false;
};

// BFS returning the shortest distance and the first-step direction index.
struct Bfs {
    int dist[10][10]{};
    uint8_t from[10][10]{};
    int bs = 10;

    // Returns distance, or 1<<20 if unreachable. Sets `first` to the
    // first-step direction (0=N,1=S,2=E,3=W) or -1 when already at target.
    int compute(int sx, int sy, int tx, int ty, int& first) {
        first = -1;
        if (sx == tx && sy == ty) return 0;
        for (int y = 0; y < bs; ++y)
            for (int x = 0; x < bs; ++x) dist[static_cast<size_t>(y)][static_cast<size_t>(x)] = 1 << 20;
        dist[static_cast<size_t>(sy)][static_cast<size_t>(sx)] = 0;
        std::queue<std::pair<int, int>> q;
        q.push({sx, sy});
        const int dx[4] = {0, 0, 1, -1};
        const int dy[4] = {-1, 1, 0, 0};
        while (!q.empty()) {
            auto [cx, cy] = q.front();
            q.pop();
            int cd = dist[static_cast<size_t>(cy)][static_cast<size_t>(cx)];
            for (int d = 0; d < 4; ++d) {
                int nx = cx + dx[static_cast<size_t>(d)];
                int ny = cy + dy[static_cast<size_t>(d)];
                if (nx < 0 || nx >= bs || ny < 0 || ny >= bs) continue;
                if (dist[static_cast<size_t>(ny)][static_cast<size_t>(nx)] != 1 << 20) continue;
                dist[static_cast<size_t>(ny)][static_cast<size_t>(nx)] = cd + 1;
                from[static_cast<size_t>(ny)][static_cast<size_t>(nx)] = static_cast<uint8_t>(d);
                if (nx == tx && ny == ty) {
                    int px = tx, py = ty;
                    while (dist[static_cast<size_t>(py)][static_cast<size_t>(px)] > 1) {
                        int d2 = from[static_cast<size_t>(py)][static_cast<size_t>(px)];
                        px -= dx[static_cast<size_t>(d2)];
                        py -= dy[static_cast<size_t>(d2)];
                    }
                    first = from[static_cast<size_t>(py)][static_cast<size_t>(px)];
                    return dist[static_cast<size_t>(ty)][static_cast<size_t>(tx)];
                }
                q.push({nx, ny});
            }
        }
        return 1 << 20;
    }
};

int inv_sum(const std::array<int, ITEM_COUNT>& inv) {
    int s = 0;
    for (int i = 0; i < ITEM_COUNT; ++i) s += inv[static_cast<size_t>(i)];
    return s;
}

}  // namespace

ActionInput build_actions(const GameState& state, int player, const Hyperparameters& hp,
                          const ProductionPlan& plan) {
    ActionInput out{};
    const Farm& farm = state.farms[static_cast<size_t>(player)];
    const PrivateState& priv = state.privates[static_cast<size_t>(player)];
    const GameConfig& cfg = state.config;
    const int step = state.step;
    const int day = state.day;
    const int tpd = std::max(1, cfg.turns_per_day);
    (void)state.hour;
    const int remaining_days = std::max(0, (cfg.episode_steps - 2 - step) / tpd);

    // ------------------------------------------------------------------
    // 1. Market orders (priority: SELL, land, seeds, animals, feed, hire).
    // ------------------------------------------------------------------
    auto& orders = out.market;
    int& oc = out.market_count;
    const int order_budget = std::max(1, cfg.max_market_orders);

    // 1a. SELL: free shed capacity and time sales around town consumption.
    const int shed_total_v = [&] {
        int s = 0;
        for (int i = 0; i < ITEM_COUNT; ++i) s += priv.shed[static_cast<size_t>(i)];
        return s;
    }();
    const int animal_feed_needed = animal_count(state, player, A_GOOSE) + animal_count(state, player, A_COW) +
                                   animal_count(state, player, A_SHEEP);
    const bool after_town_tick =
        (cfg.town_shop_sell_interval > 0 && step % cfg.town_shop_sell_interval == 0) ||
        (cfg.town_center_sell_interval > 0 && step % cfg.town_center_sell_interval == 0);
    const bool shed_pressure = shed_total_v > static_cast<int>(cfg.shed_capacity * 0.8);
    const bool endgame = remaining_days <= 2;

    for (int item = 0; item < P_COUNT; ++item) {
        int qty = priv.shed[static_cast<size_t>(item)];
        if (qty <= 0) continue;
        // Never sell wheat that is needed to feed animals.
        if (item == P_WHEAT && animal_feed_needed > 0 &&
            qty - hp.wheat_feed_reserve <= animal_feed_needed)
            continue;
        if (oc >= order_budget) break;
        const MarketParams& mp = MARKET_PARAMS[static_cast<size_t>(item)];
        const int daily_demand = town_demand_per_day(static_cast<Product>(item), state.town, cfg);
        const bool premium = mp.above_target >= 1.4;
        int sell = 0;
        if (endgame) {
            sell = qty;
        } else if (shed_pressure) {
            sell = qty;
        } else if (hp.tranche_after_town && after_town_tick) {
            int per_tick = std::max(1, daily_demand / std::max(1, tpd / std::max(1, cfg.town_shop_sell_interval)));
            sell = premium ? std::min(qty, per_tick) : std::min(qty, 100);
        } else if (premium) {
            sell = 0;  // hold premium goods for the next town tick
        } else {
            sell = std::min(qty, 10);
        }
        if (sell > 0) {
            MarketOrder& o = orders[static_cast<size_t>(oc++)];
            o.type = 1;
            o.item = static_cast<int8_t>(item);
            o.n = sell;
        }
    }

    // 1b. BUY_LAND only with a comfortable cash runway and after setup days.
    const int owned = __builtin_popcount(static_cast<unsigned>(farm.unlocked_quadrants));
    if (owned < hp.land_target() && oc < order_budget && day >= 3) {
        const int cost = LAND_PRICES[owned - 1];
        if (farm.money >= 3.0 * cost) {
            MarketOrder& o = orders[static_cast<size_t>(oc++)];
            o.type = 6;
        }
    }

    // 1c. BUY_SEED for outstanding plan crops (modest, throughput-aware batch).
    for (int c = 0; c < C_COUNT && oc < order_budget; ++c) {
        const int planted = plant_count(state, player, static_cast<Crop>(c));
        const int outstanding = std::max(0, plan.plant_targets[static_cast<size_t>(c)] - planted);
        if (outstanding <= 0) continue;
        const int have = priv.seeds[static_cast<size_t>(c)];
        const int need = std::min(outstanding, 3) - have;
        if (need <= 0) continue;
        const int cost = CROPS[static_cast<size_t>(c)].seed_cost;
        if (farm.money >= cost) {
            MarketOrder& o = orders[static_cast<size_t>(oc++)];
            o.type = 2;
            o.item = static_cast<int8_t>(c);
            o.n = std::min(need, std::max(1, static_cast<int>(farm.money / std::max(1, cost))));
        }
    }

    // 1d. BUY_ANIMAL when a structure is ready and cash covers a runway.
    for (int a = 0; a < A_COUNT && oc < order_budget; ++a) {
        const int outstanding = std::max(0, plan.animal_targets[static_cast<size_t>(a)] -
                                               (animal_count(state, player, static_cast<Animal>(a)) +
                                                priv.shed[static_cast<size_t>(item_animal(static_cast<Animal>(a)))]));
        if (outstanding <= 0) continue;
        const int cost = ANIMALS[static_cast<size_t>(a)].cost;
        if (farm.money >= 2.0 * cost && has_ready_structure(state, player, static_cast<Animal>(a))) {
            MarketOrder& o = orders[static_cast<size_t>(oc++)];
            o.type = 4;
            o.item = static_cast<int8_t>(a);
            o.n = 1;
        }
    }

    // 1e. BUY_PRODUCT WHEAT only when animals need feeding and the reserve is low.
    if (animal_feed_needed > 0 && oc < order_budget) {
        const int reserve_short = hp.wheat_feed_reserve - priv.shed[static_cast<size_t>(P_WHEAT)];
        if (reserve_short > 0) {
            const int price = state.market.prices[static_cast<size_t>(P_WHEAT)];
            if (farm.money >= price) {
                MarketOrder& o = orders[static_cast<size_t>(oc++)];
                o.type = 3;
                o.item = P_WHEAT;
                o.n = 1;
            }
        }
    }

    // 1f. HIRE fills remaining slots, capped and gated on cash runway.
    {
        const int slots = std::min(2, std::max(0, order_budget - oc));
        int want = std::max(0, hp.max_hands_per_day - farm.hires_today);
        want = std::min(want, slots);
        int hire_cost = cfg.farm_hand_cost_mult * detail::fib(farm.hires_today);
        for (int i = 0; i < want && oc < order_budget; ++i) {
            if (farm.money < 100 + hire_cost) break;
            MarketOrder& o = orders[static_cast<size_t>(oc++)];
            o.type = 5;
            hire_cost = cfg.farm_hand_cost_mult * detail::fib(farm.hires_today + i + 1);
        }
    }

    // ------------------------------------------------------------------
    // 2. Farmer + hand scheduling.
    // ------------------------------------------------------------------
    // Gather tasks.
    Task tasks[MAX_TASKS];
    int task_count = 0;
    const auto add_task = [&](int x, int y, uint8_t op, int8_t arg, int prio) {
        if (task_count >= MAX_TASKS) return;
        Task& t = tasks[task_count++];
        t.x = x;
        t.y = y;
        t.op = op;
        t.arg = arg;
        t.priority = prio;
    };

    const int bs = cfg.board_size;
    for (int y = 0; y < bs; ++y)
        for (int x = 0; x < bs; ++x) {
            const Tile& t = farm.tiles[static_cast<size_t>(y)][static_cast<size_t>(x)];
            if (t.kind == TK_EMPTY) {
                // Build a structure for a planned animal first (priority 2).
                bool built_here = false;
                for (int a = 0; a < A_COUNT; ++a) {
                    const int need = plan.animal_targets[static_cast<size_t>(a)];
                    if (need <= 0) continue;
                    const int want_kind = ANIMALS[static_cast<size_t>(a)].structure == 0 ? TK_COOP : TK_PASTURE;
                    if (structure_count(state, player, want_kind) >= need) continue;
                    add_task(x, y, want_kind == TK_COOP ? detail::OP_BUILD_COOP : detail::OP_BUILD_PASTURE, -1, 2);
                    built_here = true;
                    break;
                }
                if (built_here) continue;
                // Otherwise plant a planned crop that will still mature.
                for (int c = 0; c < C_COUNT; ++c) {
                    const int outstanding = std::max(0, plan.plant_targets[static_cast<size_t>(c)] - plant_count(state, player, static_cast<Crop>(c)));
                    if (outstanding > 0 && priv.seeds[static_cast<size_t>(c)] > 0 &&
                        day + CROPS[static_cast<size_t>(c)].first_yield_day < 29) {
                        add_task(x, y, detail::OP_PLANT, static_cast<int8_t>(c), 3);
                        break;
                    }
                }
            } else if (t.kind == TK_WEED) {
                add_task(x, y, detail::OP_DIG, -1, 5);
            } else if (t.is_plant()) {
                const CropData& cd = CROPS[static_cast<size_t>(t.crop)];
                if (t.consecutive_unwatered >= 1 && !t.watered_today) {
                    add_task(x, y, detail::OP_WATER, -1, 0);
                }
                if (t.yield_units > 0 && day - t.planted_day >= cd.first_yield_day) {
                    add_task(x, y, detail::OP_HARVEST, -1, 1);
                }
            } else if (t.kind == TK_COOP || t.kind == TK_PASTURE) {
                if (t.has_animal()) {
                    if (t.consecutive_unfed >= 1 && !t.fed_today) add_task(x, y, detail::OP_FEED, -1, 0);
                    if (t.yield_units > 0) add_task(x, y, detail::OP_HARVEST, -1, 1);
                    if (t.fertilizer_available) add_task(x, y, detail::OP_COLLECT_FERTILIZER, -1, 4);
                    if (!t.cared_today) add_task(x, y, detail::OP_CARE, -1, 4);
                } else {
                    // Place a planned animal into this empty structure.
                    for (int a = 0; a < A_COUNT; ++a) {
                        const int want_kind = ANIMALS[static_cast<size_t>(a)].structure == 0 ? TK_COOP : TK_PASTURE;
                        if (t.kind == want_kind && plan.animal_targets[static_cast<size_t>(a)] > 0 &&
                            priv.shed[static_cast<size_t>(item_animal(static_cast<Animal>(a)))] > 0) {
                            add_task(x, y, detail::OP_PLACE, static_cast<int8_t>(a), 2);
                            break;
                        }
                    }
                }
            }
        }

    // Shed logistics: pickup animals into inventory for placement, and wheat
    // for feeding; drop carried goods when shed-adjacent.
    const int half = bs / 2;
    for (int sy = half - 1; sy <= half; ++sy)
        for (int sx = half - 1; sx <= half; ++sx) {
            bool animal_pickup = false;
            for (int a = 0; a < A_COUNT; ++a) {
                if (plan.animal_targets[static_cast<size_t>(a)] >
                        animal_count(state, player, static_cast<Animal>(a)) &&
                    priv.shed[static_cast<size_t>(item_animal(static_cast<Animal>(a)))] > 0 &&
                    has_ready_structure(state, player, static_cast<Animal>(a))) {
                    add_task(sx, sy, detail::OP_PICKUP, static_cast<int8_t>(item_animal(static_cast<Animal>(a))), 2);
                    animal_pickup = true;
                    break;
                }
            }
            if (!animal_pickup && animal_feed_needed > 0 &&
                priv.shed[static_cast<size_t>(P_WHEAT)] > 0)
                add_task(sx, sy, detail::OP_PICKUP, P_WHEAT, 1);
        }

    bool any_carry = false;
    for (int u = 0; u <= farm.hand_count; ++u) {
        const auto& inv = priv.inventories[static_cast<size_t>(u)];
        if (inv_sum(inv) > 0) any_carry = true;
    }
    if (any_carry) {
        for (int y = half - 1; y <= half; ++y)
            for (int x = half - 1; x <= half; ++x)
                add_task(x, y, detail::OP_DROP, -1, 1);
    }

    // Assign units greedily by priority: for each priority, repeatedly pair the
    // nearest (unused unit, unassigned task) so a unit on an empty tile plants
    // there instead of chasing a far task in scan order.
    struct UnitPos {
        int idx;  // 0 farmer, 1..n hands
        int x, y;
    };
    UnitPos units[MAX_HANDS + 1];
    int unit_count = 1;
    units[0] = {0, farm.farmer_x, farm.farmer_y};
    for (int h = 0; h < farm.hand_count; ++h) {
        units[unit_count] = {h + 1, farm.hands[static_cast<size_t>(h)][0], farm.hands[static_cast<size_t>(h)][1]};
        ++unit_count;
    }

    std::array<uint8_t, MAX_HANDS + 1> used{};
    std::array<uint8_t, MAX_TASKS> task_used{};
    std::array<UnitAction, MAX_HANDS + 1> unit_actions{};

    Bfs bfs;
    bfs.bs = bs;

    for (int prio = 0; prio <= 6; ++prio) {
        while (true) {
            int best_u = -1, best_t = -1, best_d = 1 << 20, best_first = -1;
            for (int u = 0; u < unit_count; ++u) {
                if (used[static_cast<size_t>(u)]) continue;
                for (int ti = 0; ti < task_count; ++ti) {
                    const Task& task = tasks[static_cast<size_t>(ti)];
                    if (task.priority != prio || task_used[static_cast<size_t>(ti)]) continue;
                    // PLACE requires the animal in the unit's inventory; FEED
                    // requires wheat in the unit's inventory.
                    if (task.op == detail::OP_PLACE &&
                        priv.inventories[static_cast<size_t>(units[static_cast<size_t>(u)].idx)][static_cast<size_t>(task.arg)] <= 0)
                        continue;
                    if (task.op == detail::OP_FEED &&
                        priv.inventories[static_cast<size_t>(units[static_cast<size_t>(u)].idx)][static_cast<size_t>(P_WHEAT)] <= 0)
                        continue;
                    int first = -1;
                    int d = bfs.compute(units[static_cast<size_t>(u)].x, units[static_cast<size_t>(u)].y,
                                        task.x, task.y, first);
                    if (d < best_d) {
                        best_d = d;
                        best_u = u;
                        best_t = ti;
                        best_first = first;
                    }
                }
            }
            if (best_u < 0 || best_d >= 100) break;
            used[static_cast<size_t>(best_u)] = 1;
            task_used[static_cast<size_t>(best_t)] = 1;
            UnitAction& ua = unit_actions[static_cast<size_t>(best_u)];
            const Task& task = tasks[static_cast<size_t>(best_t)];
            if (best_d == 0) {
                ua.op = task.op;
                ua.crop = task.arg >= 0 && task.arg < C_COUNT ? task.arg : -1;
                ua.item = task.arg;
                ua.n = 1;
            } else {
                if (best_first == 0) ua.op = detail::OP_NORTH;
                else if (best_first == 1) ua.op = detail::OP_SOUTH;
                else if (best_first == 2) ua.op = detail::OP_EAST;
                else if (best_first == 3) ua.op = detail::OP_WEST;
                else ua.op = detail::OP_PASS;
            }
        }
    }

    // Units not assigned: default to PASS.
    for (int u = 0; u < unit_count; ++u)
        if (!used[static_cast<size_t>(u)]) unit_actions[static_cast<size_t>(u)].op = detail::OP_PASS;

    // Build the action input.
    out.farmer = unit_actions[0];

    if (std::getenv("KAG_DEBUG")) {
        fprintf(stderr, "[kag] p=%d step=%d farmer=(%d,%d) tasks=%d farmer_op=%d\n",
                player, step, farm.farmer_x, farm.farmer_y, task_count, out.farmer.op);
        for (int prio = 0; prio <= 6; ++prio) {
            int c = 0;
            for (int ti = 0; ti < task_count; ++ti)
                if (tasks[static_cast<size_t>(ti)].priority == prio) ++c;
            if (c) fprintf(stderr, "[kag]   prio %d: %d tasks\n", prio, c);
        }
    }

    out.hand_count = std::min<int>(farm.hand_count, MAX_HANDS);
    for (int h = 0; h < out.hand_count; ++h)
        out.hands[static_cast<size_t>(h)] = unit_actions[static_cast<size_t>(h + 1)];

    return out;
}

}  // namespace kag
