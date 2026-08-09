#include "kaggriculture_internal.hpp"

#include <string>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "board.hpp"
#include "constants.hpp"
#include "engine.hpp"
#include "plan.hpp"

namespace py = pybind11;

namespace kag {
namespace {

int product_from_name(const std::string& name) {
    for (int i = 0; i < P_COUNT; ++i)
        if (name == PRODUCT_NAMES[static_cast<size_t>(i)]) return i;
    return -1;
}

int crop_from_name(const std::string& name) {
    for (int i = 0; i < C_COUNT; ++i)
        if (name == PRODUCT_NAMES[static_cast<size_t>(i)]) return i;
    return -1;
}

int animal_from_name(const std::string& name) {
    if (name == "GOOSE") return A_GOOSE;
    if (name == "COW") return A_COW;
    if (name == "SHEEP") return A_SHEEP;
    return -1;
}

int shop_from_name(const std::string& name) {
    static const char* names[S_COUNT] = {
        "BAKERY", "PIZZA_SHOP", "BRUNCH_SPOT", "YARN_STORE",
        "ICE_CREAM_SHOP", "PET_CAFE", "SMOOTHIE_SHOP", "FARMERS_MARKET",
    };
    for (int i = 0; i < S_COUNT; ++i)
        if (name == names[i]) return i;
    return -1;
}

py::dict as_dict(py::handle obj) {
    if (py::isinstance<py::dict>(obj)) return obj.cast<py::dict>();
    py::dict out;
    for (auto item : obj.attr("__dict__").cast<py::dict>()) out[item.first] = item.second;
    return out;
}

void parse_tile(const py::object& cell, Tile& t) {
    if (cell.is_none()) {
        t.kind = TK_EMPTY;
        return;
    }
    if (py::isinstance<py::str>(cell)) {
        t.kind = (cell.cast<std::string>() == "LOCKED") ? TK_LOCKED : TK_EMPTY;
        return;
    }
    py::dict d = as_dict(cell);
    std::string kind = d["kind"].cast<std::string>();
    if (kind == "WEED") {
        t.kind = TK_WEED;
    } else if (kind == "PLANT") {
        t.kind = TK_PLANT;
        t.crop = static_cast<uint8_t>(crop_from_name(d["crop"].cast<std::string>()));
        t.planted_day = static_cast<int16_t>(d["planted_day"].cast<int>());
        t.watered_today = d["watered_today"].cast<bool>() ? 1 : 0;
        t.consecutive_unwatered = static_cast<uint8_t>(d["consecutive_unwatered"].cast<int>());
        t.yield_units = static_cast<int16_t>(d["yield_units"].cast<int>());
        t.max_lifespan_step = d["max_lifespan_step"].cast<int>();
        t.fertilized_until_day = static_cast<int16_t>(d["fertilized_until_day"].cast<int>());
    } else if (kind == "COOP" || kind == "PASTURE") {
        t.kind = (kind == "COOP") ? TK_COOP : TK_PASTURE;
        if (d.contains("animal") && !d["animal"].is_none()) {
            t.animal = static_cast<uint8_t>(animal_from_name(d["animal"].cast<std::string>()));
            t.placed_day = static_cast<int16_t>(d["placed_day"].cast<int>());
            t.yield_units = static_cast<int16_t>(d["yield_units"].cast<int>());
            t.fed_today = d["fed_today"].cast<bool>() ? 1 : 0;
            t.consecutive_unfed = static_cast<uint8_t>(d["consecutive_unfed"].cast<int>());
            t.cared_today = d["cared_today"].cast<bool>() ? 1 : 0;
            t.fertilizer_available = d["fertilizer_available"].cast<bool>() ? 1 : 0;
            t.pending_care_bonus = static_cast<int16_t>(d["pending_care_bonus"].cast<int>());
        }
    }
}

int quadrant_bit(const std::string& q) {
    if (q == "NW") return 0x1;
    if (q == "NE") return 0x2;
    if (q == "SW") return 0x4;
    if (q == "SE") return 0x8;
    return 0;
}

void parse_game_state(const py::object& obs, const py::object& config, GameState& gs) {
    py::dict o = as_dict(obs);
    gs = GameState{};
    GameConfig& cfg = gs.config;

    if (!config.is_none()) {
        py::dict c = as_dict(config);
        auto geti = [&](const char* key, int def) {
            return c.contains(key) ? c[key].cast<int>() : def;
        };
        cfg.episode_steps = geti("episodeSteps", DEFAULT_EPISODE_STEPS);
        cfg.board_size = geti("boardSize", DEFAULT_BOARD_SIZE);
        cfg.starting_money = geti("startingMoney", DEFAULT_STARTING_MONEY);
        cfg.max_market_orders = geti("maxMarketOrdersPerTurn", DEFAULT_MAX_MARKET_ORDERS);
        cfg.turns_per_day = geti("turnsPerDay", DEFAULT_TURNS_PER_DAY);
        cfg.shed_capacity = geti("shedCapacity", DEFAULT_SHED_CAPACITY);
        cfg.weed_spawn_chance = c.contains("weedSpawnChance")
                                   ? c["weedSpawnChance"].cast<double>()
                                   : DEFAULT_WEED_SPAWN_CHANCE;
        cfg.town_shop_unlock_interval = geti("townShopUnlockInterval", DEFAULT_TOWN_SHOP_UNLOCK_INTERVAL);
        cfg.town_shop_sell_interval = geti("townShopSellInterval", DEFAULT_TOWN_SHOP_SELL_INTERVAL);
        cfg.town_center_sell_interval = geti("townCenterSellInterval", DEFAULT_TOWN_CENTER_SELL_INTERVAL);
        cfg.farm_hand_cost_mult = geti("farmHandCostMult", DEFAULT_FARM_HAND_COST_MULT);
    }

    gs.player = o.contains("player") ? o["player"].cast<int>() : 0;
    gs.step = o.contains("step") ? o["step"].cast<int>() : 0;
    gs.day = o.contains("day") ? o["day"].cast<int>() : 0;
    gs.hour = o.contains("hour") ? o["hour"].cast<int>() : 0;
    gs.done = false;

    // Market.
    py::dict market = as_dict(o["market"]);
    py::dict inv = as_dict(market["inventory"]);
    py::dict prices = as_dict(market["prices"]);
    for (auto item : inv) {
        int idx = product_from_name(item.first.cast<std::string>());
        if (idx >= 0) gs.market.inventory[static_cast<size_t>(idx)] = item.second.cast<int>();
    }
    for (auto item : prices) {
        int idx = product_from_name(item.first.cast<std::string>());
        if (idx >= 0) gs.market.prices[static_cast<size_t>(idx)] = item.second.cast<int>();
    }

    // Town.
    py::dict town = as_dict(o["town"]);
    if (town.contains("unlocked_shops")) {
        for (auto item : town["unlocked_shops"].cast<py::list>()) {
            int s = shop_from_name(item.cast<std::string>());
            if (s >= 0 && gs.town.shop_count < MAX_SHOP_INSTANCES)
                gs.town.unlocked_shops[static_cast<size_t>(gs.town.shop_count++)] = static_cast<uint8_t>(s);
        }
    }

    // Farms.
    py::list farms = o["farms"].cast<py::list>();
    for (int p = 0; p < 2 && p < static_cast<int>(farms.size()); ++p) {
        py::dict f = as_dict(farms[p]);
        Farm& farm = gs.farms[static_cast<size_t>(p)];
        farm.board_size = cfg.board_size;
        farm.money = f.contains("money") ? f["money"].cast<double>() : 0.0;
        py::list tiles = f["tiles"].cast<py::list>();
        for (int y = 0; y < cfg.board_size && y < static_cast<int>(tiles.size()); ++y) {
            py::list row = tiles[y].cast<py::list>();
            for (int x = 0; x < cfg.board_size && x < static_cast<int>(row.size()); ++x)
                parse_tile(row[x], farm.tiles[static_cast<size_t>(y)][static_cast<size_t>(x)]);
        }
        py::list farmer = f["farmer"].cast<py::list>();
        farm.farmer_x = farmer[0].cast<int>();
        farm.farmer_y = farmer[1].cast<int>();
        if (f.contains("hands")) {
            py::list hands = f["hands"].cast<py::list>();
            farm.hand_count = std::min<int>(static_cast<int>(hands.size()), MAX_HANDS);
            for (int h = 0; h < farm.hand_count; ++h) {
                py::list pos = hands[h].cast<py::list>();
                farm.hands[static_cast<size_t>(h)][0] = static_cast<int16_t>(pos[0].cast<int>());
                farm.hands[static_cast<size_t>(h)][1] = static_cast<int16_t>(pos[1].cast<int>());
            }
        }
        farm.unlocked_quadrants = 0;
        if (f.contains("unlocked_quadrants")) {
            for (auto item : f["unlocked_quadrants"].cast<py::list>())
                farm.unlocked_quadrants |= static_cast<uint8_t>(quadrant_bit(item.cast<std::string>()));
        }
        farm.hires_today = f.contains("hires_today") ? f["hires_today"].cast<int>() : 0;
    }

    // Own private state.
    if (o.contains("private") && !o["private"].is_none()) {
        py::dict priv = as_dict(o["private"]);
        PrivateState& ps = gs.privates[static_cast<size_t>(gs.player)];
        if (priv.contains("shed")) {
            for (auto item : as_dict(priv["shed"])) {
                const std::string name = item.first.cast<std::string>();
                int value = item.second.cast<int>();
                int prod = product_from_name(name);
                if (prod >= 0) ps.shed[static_cast<size_t>(prod)] = value;
                else {
                    int a = animal_from_name(name);
                    if (a >= 0) ps.shed[static_cast<size_t>(item_animal(static_cast<Animal>(a)))] = value;
                }
            }
        }
        if (priv.contains("seeds")) {
            for (auto item : as_dict(priv["seeds"])) {
                int c = crop_from_name(item.first.cast<std::string>());
                if (c >= 0) ps.seeds[static_cast<size_t>(c)] = item.second.cast<int>();
            }
        }
        if (priv.contains("inventories")) {
            py::list invs = priv["inventories"].cast<py::list>();
            for (int u = 0; u < static_cast<int>(invs.size()) && u < MAX_HANDS + 1; ++u) {
                for (auto item : as_dict(invs[u])) {
                    const std::string name = item.first.cast<std::string>();
                    int value = item.second.cast<int>();
                    int prod = product_from_name(name);
                    if (prod >= 0) ps.inventories[static_cast<size_t>(u)][static_cast<size_t>(prod)] = value;
                    else {
                        int a = animal_from_name(name);
                        if (a >= 0) ps.inventories[static_cast<size_t>(u)][static_cast<size_t>(item_animal(static_cast<Animal>(a)))] = value;
                    }
                }
            }
        }
    }
}

const char* item_name(int item);

py::list unit_action_to_py(const UnitAction& ua) {
    py::list out;
    switch (ua.op) {
        case detail::OP_NORTH: out.append("NORTH"); break;
        case detail::OP_SOUTH: out.append("SOUTH"); break;
        case detail::OP_EAST: out.append("EAST"); break;
        case detail::OP_WEST: out.append("WEST"); break;
        case detail::OP_PICKUP: out.append("PICKUP"); out.append(item_name(ua.item)); out.append(ua.n); break;
        case detail::OP_DROP: out.append("DROP"); break;
        case detail::OP_PLANT: out.append("PLANT"); out.append(PRODUCT_NAMES[static_cast<size_t>(ua.crop)]); break;
        case detail::OP_WATER: out.append("WATER"); break;
        case detail::OP_HARVEST: out.append("HARVEST"); break;
        case detail::OP_FERTILIZE: out.append("FERTILIZE"); break;
        case detail::OP_BUILD_COOP: out.append("BUILD_COOP"); break;
        case detail::OP_BUILD_PASTURE: out.append("BUILD_PASTURE"); break;
        case detail::OP_DIG: out.append("DIG"); break;
        case detail::OP_PLACE: out.append("PLACE"); out.append(item_name(ua.item)); out.append(ua.n); break;
        case detail::OP_FEED: out.append("FEED"); break;
        case detail::OP_COLLECT_FERTILIZER: out.append("COLLECT_FERTILIZER"); break;
        case detail::OP_CARE: out.append("CARE"); break;
        default: out.append("PASS"); break;
    }
    return out;
}

const char* item_name(int item) {
    if (item < P_COUNT) return PRODUCT_NAMES[static_cast<size_t>(item)];
    switch (item - P_COUNT) {
        case A_GOOSE: return "GOOSE";
        case A_COW: return "COW";
        default: return "SHEEP";
    }
}

py::dict action_to_py(const ActionInput& a) {
    py::dict out;
    out["farmer"] = unit_action_to_py(a.farmer);
    py::list hands;
    for (int h = 0; h < a.hand_count; ++h) hands.append(unit_action_to_py(a.hands[static_cast<size_t>(h)]));
    out["hands"] = hands;
    py::list market;
    for (int i = 0; i < a.market_count; ++i) {
        const MarketOrder& o = a.market[static_cast<size_t>(i)];
        py::list row;
        if (o.type == 1) { row.append("SELL"); row.append(item_name(o.item)); row.append(o.n); }
        else if (o.type == 2) { row.append("BUY_SEED"); row.append(PRODUCT_NAMES[static_cast<size_t>(o.item)]); row.append(o.n); }
        else if (o.type == 3) { row.append("BUY_PRODUCT"); row.append(item_name(o.item)); row.append(o.n); }
        else if (o.type == 4) { row.append("BUY_ANIMAL"); row.append(item_name(item_animal(static_cast<Animal>(o.item)))); row.append(o.n); }
        else if (o.type == 5) { row.append("HIRE"); }
        else if (o.type == 6) { row.append("BUY_LAND"); }
        else continue;
        market.append(row);
    }
    out["market"] = market;
    return out;
}

class PyEngine {
public:
    KaggricultureEngine engine{};
    GameState last_state{};

    void update_observation(const py::object& obs, const py::object& config) {
        parse_game_state(obs, config, last_state);
        engine.update_observation(last_state);
    }

    py::dict choose_actions(int time_budget_ms = 900) {
        ActionInput actions = engine.choose_actions(time_budget_ms);
        return action_to_py(actions);
    }

    void set_hyperparameters(const py::dict& params) {
        Hyperparameters h = engine.hp;
        auto seti = [&](const char* key, int& field) {
            if (params.contains(key)) field = params[key].cast<int>();
        };
        auto setf = [&](const char* key, double& field) {
            if (params.contains(key)) field = params[key].cast<double>();
        };
        seti("target_cows", h.target_cows);
        seti("target_sheep", h.target_sheep);
        seti("target_geese", h.target_geese);
        seti("wheat_tiles", h.wheat_tiles);
        seti("carrot_tiles", h.carrot_tiles);
        seti("melon_tiles", h.melon_tiles);
        seti("tomato_tiles", h.tomato_tiles);
        seti("strawberry_tiles", h.strawberry_tiles);
        seti("buy_land_ne", h.buy_land_ne);
        seti("buy_land_sw", h.buy_land_sw);
        seti("buy_land_se", h.buy_land_se);
        seti("max_hands_per_day", h.max_hands_per_day);
        seti("shed_keep", h.shed_keep);
        seti("tranche_after_town", h.tranche_after_town);
        seti("wheat_feed_reserve", h.wheat_feed_reserve);
        setf("deny_threshold", h.deny_threshold);
        engine.set_hyperparameters(h);
    }

    py::dict get_hyperparameters() {
        const Hyperparameters& h = engine.hp;
        py::dict out;
        out["target_cows"] = h.target_cows;
        out["target_sheep"] = h.target_sheep;
        out["target_geese"] = h.target_geese;
        out["wheat_tiles"] = h.wheat_tiles;
        out["carrot_tiles"] = h.carrot_tiles;
        out["melon_tiles"] = h.melon_tiles;
        out["tomato_tiles"] = h.tomato_tiles;
        out["strawberry_tiles"] = h.strawberry_tiles;
        out["buy_land_ne"] = h.buy_land_ne;
        out["buy_land_sw"] = h.buy_land_sw;
        out["buy_land_se"] = h.buy_land_se;
        out["max_hands_per_day"] = h.max_hands_per_day;
        out["shed_keep"] = h.shed_keep;
        out["tranche_after_town"] = h.tranche_after_town;
        out["wheat_feed_reserve"] = h.wheat_feed_reserve;
        out["deny_threshold"] = h.deny_threshold;
        return out;
    }

    py::dict debug_state() {
        const GameState& gs = engine.state;
        py::dict out;
        out["step"] = gs.step;
        out["day"] = gs.day;
        out["hour"] = gs.hour;
        out["money"] = gs.farms[static_cast<size_t>(gs.player)].money;
        py::list farm_tiles;
        const Farm& farm = gs.farms[static_cast<size_t>(gs.player)];
        for (int y = 0; y < gs.config.board_size; ++y) {
            py::list row;
            for (int x = 0; x < gs.config.board_size; ++x) {
                const Tile& t = farm.tiles[static_cast<size_t>(y)][static_cast<size_t>(x)];
                if (t.kind == TK_EMPTY) row.append(py::none());
                else if (t.kind == TK_LOCKED) row.append("LOCKED");
                else if (t.kind == TK_WEED) row.append("WEED");
                else if (t.kind == TK_PLANT) {
                    py::dict d;
                    d["crop"] = PRODUCT_NAMES[static_cast<size_t>(t.crop)];
                    d["yield_units"] = t.yield_units;
                    row.append(d);
                } else {
                    py::dict d;
                    d["kind"] = t.kind == TK_COOP ? "COOP" : "PASTURE";
                    if (t.has_animal()) d["animal"] = item_name(item_animal(static_cast<Animal>(t.animal)));
                    row.append(d);
                }
            }
            farm_tiles.append(row);
        }
        out["tiles"] = farm_tiles;
        out["plan_ready"] = engine.plan_ready;
        out["projected_value"] = engine.plan.projected_value;
        py::list targets;
        for (int a = 0; a < A_COUNT; ++a) targets.append(engine.plan.animal_targets[static_cast<size_t>(a)]);
        out["animal_targets"] = targets;
        out["shed_animal_cows"] = gs.privates[static_cast<size_t>(gs.player)].shed[static_cast<size_t>(item_animal(A_COW))];
        out["farm_cows"] = animal_count(gs, gs.player, A_COW);
        return out;
    }
};

}  // namespace
}  // namespace kag

PYBIND11_MODULE(kaggriculture_engine, m) {
    m.doc() = "Kaggriculture Simulated Planning native engine";
    py::class_<kag::PyEngine>(m, "Engine")
        .def(py::init<>())
        .def("update_observation", &kag::PyEngine::update_observation, py::arg("obs"), py::arg("config") = py::none())
        .def("choose_actions", &kag::PyEngine::choose_actions, py::arg("time_budget_ms") = 900)
        .def("set_hyperparameters", &kag::PyEngine::set_hyperparameters)
        .def("get_hyperparameters", &kag::PyEngine::get_hyperparameters)
        .def("debug_state", &kag::PyEngine::debug_state);
    m.def("market_price", [](const std::string& item, int inventory) {
        int idx = kag::product_from_name(item);
        return idx >= 0 ? kag::detail::market_price(static_cast<kag::Product>(idx), inventory) : -1;
    });
    m.def("shape_value", [](const std::string& func, double x) {
        auto f = [&]() -> kag::PriceFunc {
            if (func == "sq") return kag::F_SQ;
            if (func == "sqrt") return kag::F_SQRT;
            if (func == "log") return kag::F_LOG;
            if (func == "log10") return kag::F_LOG10;
            return kag::F_LINEAR;
        };
        return kag::detail::shape(f(), x);
    });
}
