#pragma once

#include <array>

#include "board.hpp"

namespace kag {

// Tunable production plan and market-policy hyperparameters.
struct Hyperparameters {
    int target_cows = 0;
    int target_sheep = 0;
    int target_geese = 0;
    int wheat_tiles = 24;
    int carrot_tiles = 6;
    int melon_tiles = 0;
    int tomato_tiles = 0;
    int strawberry_tiles = 0;
    int buy_land_ne = 0;
    int buy_land_sw = 0;
    int buy_land_se = 0;
    int max_hands_per_day = 6;
    int shed_keep = 20;
    int tranche_after_town = 1;
    int wheat_feed_reserve = 0;
    double deny_threshold = 1.0;
    double opponent_scale = 1.0;  // estimated opponent production multiplier

    int land_target() const { return 1 + buy_land_ne + buy_land_sw + buy_land_se; }
};

// Long-horizon production plan evaluated by simulation.
struct ProductionPlan {
    Hyperparameters hp{};
    // Per-crop target tile counts that still need planting.
    std::array<int, C_COUNT> plant_targets{};
    // Animal purchase targets still outstanding.
    std::array<int, A_COUNT> animal_targets{};
    double projected_value = 0.0;
};

class KaggricultureEngine {
public:
    GameState state{};
    Hyperparameters hp{};
    bool plan_ready = false;
    ProductionPlan plan{};

    void reset();
    void update_observation(const GameState& obs);
    ActionInput choose_actions(int time_budget_ms);
    void set_hyperparameters(const Hyperparameters& h);

    void replan();
};

}  // namespace kag
