#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>

#include "constants.hpp"

namespace kag {

enum TileKind : uint8_t {
    TK_EMPTY = 0,
    TK_LOCKED,
    TK_WEED,
    TK_PLANT,
    TK_COOP,
    TK_PASTURE,
};

struct Tile {
    uint8_t kind = TK_EMPTY;
    uint8_t crop = 0;               // Crop for TK_PLANT
    int16_t planted_day = 0;
    uint8_t watered_today = 0;
    uint8_t consecutive_unwatered = 0;
    int16_t yield_units = 0;
    int32_t max_lifespan_step = -1; // -1 for ongoing crops before cap
    int16_t fertilized_until_day = -1;
    uint8_t animal = 255;           // Animal for TK_COOP/TK_PASTURE, 255 = empty
    int16_t placed_day = 0;
    uint8_t fed_today = 0;
    uint8_t consecutive_unfed = 0;
    uint8_t cared_today = 0;
    uint8_t fertilizer_available = 0;
    int16_t pending_care_bonus = 0;

    bool is_plant() const { return kind == TK_PLANT; }
    bool has_animal() const { return animal != 255; }
};

struct PrivateState {
    std::array<int, ITEM_COUNT> shed{};
    std::array<int, C_COUNT> seeds{};
    std::array<std::array<int, ITEM_COUNT>, MAX_HANDS + 1> inventories{};
};

struct Farm {
    double money = 0.0;
    std::array<std::array<Tile, 16>, 16> tiles{};
    int board_size = 10;
    int farmer_x = 0;
    int farmer_y = 0;
    std::array<std::array<int16_t, 2>, MAX_HANDS> hands{};
    int hand_count = 0;
    uint8_t unlocked_quadrants = 0x1;  // bit0=NW, bit1=NE, bit2=SW, bit3=SE
    int hires_today = 0;

    void reset_tiles() {
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
                tiles[static_cast<size_t>(y)][static_cast<size_t>(x)] = Tile{};
    }
};

struct MarketState {
    std::array<int, P_COUNT> inventory{};
    std::array<int, P_COUNT> prices{};
};

struct TownState {
    std::array<uint8_t, MAX_SHOP_INSTANCES> unlocked_shops{};
    int shop_count = 0;
};

struct GameConfig {
    int episode_steps = DEFAULT_EPISODE_STEPS;
    int board_size = DEFAULT_BOARD_SIZE;
    int starting_money = DEFAULT_STARTING_MONEY;
    int max_market_orders = DEFAULT_MAX_MARKET_ORDERS;
    int turns_per_day = DEFAULT_TURNS_PER_DAY;
    int shed_capacity = DEFAULT_SHED_CAPACITY;
    double weed_spawn_chance = DEFAULT_WEED_SPAWN_CHANCE;
    int town_shop_unlock_interval = DEFAULT_TOWN_SHOP_UNLOCK_INTERVAL;
    int town_shop_sell_interval = DEFAULT_TOWN_SHOP_SELL_INTERVAL;
    int town_center_sell_interval = DEFAULT_TOWN_CENTER_SELL_INTERVAL;
    int farm_hand_cost_mult = DEFAULT_FARM_HAND_COST_MULT;
};

// Decoded per-turn actions.
struct UnitAction {
    uint8_t op = 0;                 // opcode, see kaggriculture_internal.hpp
    int8_t crop = -1;               // for PLANT
    int8_t item = -1;               // for PICKUP/PLACE item (Product or Animal index)
    int n = 1;
};

struct MarketOrder {
    uint8_t type = 0;               // 0 none, 1 SELL, 2 BUY_SEED, 3 BUY_PRODUCT, 4 BUY_ANIMAL, 5 HIRE, 6 BUY_LAND
    int8_t item = -1;
    int n = 0;
};

struct ActionInput {
    UnitAction farmer{};
    std::array<UnitAction, MAX_HANDS> hands{};
    int hand_count = 0;
    std::array<MarketOrder, 16> market{};
    int market_count = 0;
};

struct GameState {
    std::array<Farm, 2> farms{};
    std::array<PrivateState, 2> privates{};
    MarketState market{};
    TownState town{};
    GameConfig config{};
    int player = 0;      // controlled player id
    int step = 0;
    int day = 0;
    int hour = 0;
    bool done = false;

    int player_id() const { return player; }
};

}  // namespace kag
