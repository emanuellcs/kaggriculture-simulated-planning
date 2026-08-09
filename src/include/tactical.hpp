#pragma once

#include "board.hpp"
#include "engine.hpp"
#include "plan.hpp"

namespace kag {

// Build the per-turn action dict for one player: farmer op, hand ops, and
// market orders, using the current plan and market forecast.
ActionInput build_actions(const GameState& state, int player, const Hyperparameters& hp,
                          const ProductionPlan& plan);

}  // namespace kag
