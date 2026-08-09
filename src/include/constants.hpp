#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace kag {

// Product and entity enums mirror the Kaggle engine ordering.
enum Product : uint8_t {
    P_WHEAT = 0,
    P_CARROT,
    P_TOMATO,
    P_STRAWBERRY,
    P_MELON,
    P_EGG,
    P_MILK,
    P_WOOL,
    P_FERTILIZER,
    P_COUNT,
};

enum Crop : uint8_t {
    C_WHEAT = P_WHEAT,
    C_CARROT = P_CARROT,
    C_TOMATO = P_TOMATO,
    C_STRAWBERRY = P_STRAWBERRY,
    C_MELON = P_MELON,
    C_COUNT,
};

enum Animal : uint8_t {
    A_GOOSE = 0,
    A_COW,
    A_SHEEP,
    A_COUNT,
};

// Unified shed/inventory item space: products 0..8, then animals 9..11.
constexpr int ITEM_COUNT = static_cast<int>(P_COUNT) + static_cast<int>(A_COUNT);
constexpr int ITEM_A_GOOSE = static_cast<int>(P_COUNT) + A_GOOSE;
constexpr int ITEM_A_COW = static_cast<int>(P_COUNT) + A_COW;
constexpr int ITEM_A_SHEEP = static_cast<int>(P_COUNT) + A_SHEEP;
constexpr int item_animal(Animal a) { return static_cast<int>(P_COUNT) + static_cast<int>(a); }

enum Shop : uint8_t {
    S_BAKERY = 0,
    S_PIZZA_SHOP,
    S_BRUNCH_SPOT,
    S_YARN_STORE,
    S_ICE_CREAM_SHOP,
    S_PET_CAFE,
    S_SMOOTHIE_SHOP,
    S_FARMERS_MARKET,
    S_COUNT,
};

enum PriceFunc : uint8_t { F_LINEAR = 0, F_SQ, F_SQRT, F_LOG, F_LOG10 };

struct CropData {
    int seed_cost;
    int first_yield_day;
    int max_yield_day;
    int interval;      // 0 for one-time crops
    int max_yield;
    bool ongoing;
};

struct AnimalData {
    int cost;
    int structure;     // 0 = COOP, 1 = PASTURE
    int first_yield_day;
    int interval;
    int max_held;
    Product product;
};

struct MarketParams {
    double base;
    int I0;
    int T;
    PriceFunc below_func;
    double below_target;
    PriceFunc above_func;
    double above_target;
};

constexpr const char* PRODUCT_NAMES[P_COUNT] = {
    "WHEAT", "CARROT", "TOMATO", "STRAWBERRY", "MELON", "EGG", "MILK", "WOOL", "FERTILIZER",
};

constexpr CropData CROPS[C_COUNT] = {
    /*WHEAT*/ {10, 2, 4, 0, 6, false},
    /*CARROT*/ {20, 2, 3, 0, 4, false},
    /*TOMATO*/ {50, 8, 8, 1, 4, true},
    /*STRAWBERRY*/ {100, 10, 10, 2, 4, true},
    /*MELON*/ {80, 10, 12, 0, 6, false},
};

constexpr AnimalData ANIMALS[A_COUNT] = {
    /*GOOSE*/ {300, 0, 4, 1, 4, P_EGG},
    /*COW*/ {400, 1, 8, 2, 6, P_MILK},
    /*SHEEP*/ {500, 1, 6, 3, 6, P_WOOL},
};

constexpr int MARKET_I0 = 10000;
constexpr int PRICE_FLOOR = 1;

constexpr MarketParams MARKET_PARAMS[P_COUNT] = {
    /*WHEAT*/ {25, MARKET_I0, 400, F_SQRT, 0.80, F_LOG, 0.20},
    /*CARROT*/ {35, MARKET_I0, 450, F_LOG, 0.20, F_SQRT, 0.70},
    /*TOMATO*/ {60, MARKET_I0, 200, F_LINEAR, 0.40, F_SQRT, 0.60},
    /*STRAWBERRY*/ {120, MARKET_I0, 100, F_SQRT, 0.70, F_LINEAR, 1.60},
    /*MELON*/ {250, MARKET_I0, 300, F_LOG, 0.20, F_SQ, 3.60},
    /*EGG*/ {50, MARKET_I0, 332, F_LINEAR, 0.40, F_LOG, 0.20},
    /*MILK*/ {160, MARKET_I0, 122, F_SQRT, 0.60, F_LINEAR, 1.60},
    /*WOOL*/ {200, MARKET_I0, 105, F_LOG, 0.20, F_SQ, 3.20},
    /*FERTILIZER*/ {100, MARKET_I0, 200, F_LINEAR, 0.40, F_LINEAR, 0.40},
};

// Shop -> demanded products (packed into a fixed mask of products).
constexpr uint16_t SHOP_PRODUCTS[S_COUNT] = {
    (1u << P_EGG) | (1u << P_WHEAT),                                   // BAKERY
    (1u << P_MILK) | (1u << P_TOMATO) | (1u << P_WHEAT),               // PIZZA_SHOP
    (1u << P_EGG) | (1u << P_WHEAT) | (1u << P_STRAWBERRY),            // BRUNCH_SPOT
    (1u << P_WOOL),                                                    // YARN_STORE (single)
    (1u << P_STRAWBERRY) | (1u << P_MILK) | (1u << P_WHEAT),           // ICE_CREAM_SHOP
    (1u << P_CARROT),                                                  // PET_CAFE (single)
    (1u << P_STRAWBERRY) | (1u << P_MILK),                             // SMOOTHIE_SHOP
    (1u << P_WHEAT) | (1u << P_CARROT) | (1u << P_TOMATO) | (1u << P_STRAWBERRY),  // FARMERS_MARKET
};

constexpr int LAND_PRICES[3] = {1000, 2000, 4000};

// Configuration defaults (overridable per episode).
constexpr int DEFAULT_EPISODE_STEPS = 720;
constexpr int DEFAULT_BOARD_SIZE = 10;
constexpr int DEFAULT_STARTING_MONEY = 3000;
constexpr int DEFAULT_MAX_MARKET_ORDERS = 10;
constexpr int DEFAULT_TURNS_PER_DAY = 24;
constexpr int DEFAULT_SHED_CAPACITY = 100;
constexpr double DEFAULT_WEED_SPAWN_CHANCE = 0.005;
constexpr int DEFAULT_TOWN_SHOP_UNLOCK_INTERVAL = 3;
constexpr int DEFAULT_TOWN_SHOP_SELL_INTERVAL = 4;
constexpr int DEFAULT_TOWN_CENTER_SELL_INTERVAL = 24;
constexpr int DEFAULT_FARM_HAND_COST_MULT = 1;
constexpr int MAX_SHOP_INSTANCES = 8;
constexpr int MAX_HANDS = 24;

// Movement offsets; y grows downward.
constexpr int DIR_DX[4] = {0, 0, 1, -1};  // N, S, E, W
constexpr int DIR_DY[4] = {-1, 1, 0, 0};

}  // namespace kag
