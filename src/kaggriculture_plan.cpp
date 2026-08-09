#include "kaggriculture_internal.hpp"

#include <algorithm>

#include "board.hpp"
#include "constants.hpp"
#include "engine.hpp"
#include "plan.hpp"
#include "tactical.hpp"

namespace kag {

int plant_count(const GameState& state, int player, Crop crop) {
    const Farm& farm = state.farms[static_cast<size_t>(player)];
    int count = 0;
    for (int y = 0; y < state.config.board_size; ++y)
        for (int x = 0; x < state.config.board_size; ++x) {
            const Tile& t = farm.tiles[static_cast<size_t>(y)][static_cast<size_t>(x)];
            if (t.is_plant() && t.crop == static_cast<uint8_t>(crop)) ++count;
        }
    return count;
}

int animal_count(const GameState& state, int player, Animal animal) {
    const Farm& farm = state.farms[static_cast<size_t>(player)];
    int count = 0;
    for (int y = 0; y < state.config.board_size; ++y)
        for (int x = 0; x < state.config.board_size; ++x) {
            const Tile& t = farm.tiles[static_cast<size_t>(y)][static_cast<size_t>(x)];
            if (t.has_animal() && t.animal == static_cast<uint8_t>(animal)) ++count;
        }
    return count;
}

bool has_ready_structure(const GameState& state, int player, Animal animal) {
    const Farm& farm = state.farms[static_cast<size_t>(player)];
    const int want_kind = ANIMALS[static_cast<size_t>(animal)].structure == 0 ? TK_COOP : TK_PASTURE;
    for (int y = 0; y < state.config.board_size; ++y)
        for (int x = 0; x < state.config.board_size; ++x) {
            const Tile& t = farm.tiles[static_cast<size_t>(y)][static_cast<size_t>(x)];
            if (t.kind == want_kind && !t.has_animal()) return true;
        }
    return false;
}

int structure_count(const GameState& state, int player, int structure_kind) {
    const Farm& farm = state.farms[static_cast<size_t>(player)];
    int count = 0;
    for (int y = 0; y < state.config.board_size; ++y)
        for (int x = 0; x < state.config.board_size; ++x) {
            const Tile& t = farm.tiles[static_cast<size_t>(y)][static_cast<size_t>(x)];
            if (t.kind == static_cast<uint8_t>(structure_kind)) ++count;
        }
    return count;
}

ProductionPlan make_plan(const GameState& state, int player, const Hyperparameters& hp) {
    (void)state;
    (void)player;
    ProductionPlan plan;
    plan.hp = hp;
    plan.plant_targets = {hp.wheat_tiles, hp.carrot_tiles, hp.tomato_tiles, hp.strawberry_tiles, hp.melon_tiles};
    plan.animal_targets = {hp.target_geese, hp.target_cows, hp.target_sheep};
    plan.projected_value = 0.0;
    return plan;
}

namespace {

// One full-season forward simulation using the per-turn tactical policy for
// both players with their respective hyperparameters. Returns final money.
double simulate_season(GameState sim, int my_player, const Hyperparameters& my_hp,
                       const Hyperparameters& opp_hp) {
    detail::Rng rng(0xC0FFEEu);
    ProductionPlan my_plan = make_plan(sim, my_player, my_hp);
    ProductionPlan opp_plan = make_plan(sim, 1 - my_player, opp_hp);
    while (!sim.done) {
        ActionInput a0 = build_actions(sim, 0, my_player == 0 ? my_hp : opp_hp,
                                       my_player == 0 ? my_plan : opp_plan);
        ActionInput a1 = build_actions(sim, 1, my_player == 1 ? my_hp : opp_hp,
                                       my_player == 1 ? my_plan : opp_plan);
        apply_turn(sim, a0, a1, rng);
    }
    return sim.farms[static_cast<size_t>(my_player)].money;
}

}  // namespace

double plan_projected_value(const GameState& state, int player, const Hyperparameters& my_hp,
                            const Hyperparameters& opp_hp) {
    const double my_money = simulate_season(state, player, my_hp, opp_hp);
    return my_money;
}

ProductionPlan plan_for_state(const GameState& state, int player, const Hyperparameters& hp) {
    // Stable production plan built directly from the tuned hyperparameters.
    // Adaptive beam re-planning can be layered on during tuning; the direct
    // route keeps targets consistent so the market/order layer does not
    // over-commit capital from day to day.
    (void)state;
    ProductionPlan plan = make_plan(state, player, hp);
    plan.projected_value = 0.0;
    return plan;
}

}  // namespace kag
