#include "kaggriculture_internal.hpp"

#include "board.hpp"
#include "constants.hpp"
#include "engine.hpp"
#include "plan.hpp"
#include "tactical.hpp"

namespace kag {

void KaggricultureEngine::reset() {
    state = GameState{};
    plan_ready = false;
    plan = ProductionPlan{};
}

void KaggricultureEngine::update_observation(const GameState& obs) {
    state = obs;
    if (!plan_ready) {
        replan();
    } else {
        // Re-plan at day boundaries or when the town unlocks a shop.
        if (obs.hour == 0) replan();
    }
}

void KaggricultureEngine::set_hyperparameters(const Hyperparameters& h) {
    hp = h;
    plan_ready = false;
    plan = ProductionPlan{};
}

void KaggricultureEngine::replan() {
    plan = plan_for_state(state, state.player_id(), hp);
    plan_ready = true;
}

ActionInput KaggricultureEngine::choose_actions(int time_budget_ms) {
    (void)time_budget_ms;
    return build_actions(state, state.player_id(), hp, plan);
}

}  // namespace kag
