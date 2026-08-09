#pragma once

#include <array>

#include "board.hpp"
#include "engine.hpp"

namespace kag {

// Count planted tiles of a crop on a player's farm.
int plant_count(const GameState& state, int player, Crop crop);
// Count live animals of a type on a player's farm.
int animal_count(const GameState& state, int player, Animal animal);
// Count structures of a kind (coop/pasture) on a player's farm.
int structure_count(const GameState& state, int player, int structure_kind);
// True when an empty matching structure exists for the animal type.
bool has_ready_structure(const GameState& state, int player, Animal animal);

// Build the per-crop and per-animal production targets from the hyperparameters
// and the current farm state (what is still outstanding).
ProductionPlan make_plan(const GameState& state, int player, const Hyperparameters& hp);

// Simulate a candidate plan to the end of the season against a model opponent
// and return the projected final money difference.
double plan_projected_value(const GameState& state, int player, const Hyperparameters& my_hp,
                            const Hyperparameters& opp_hp);

// Adaptive plan search: evaluate a small beam of plan variants and select the
// one with the highest projected value for this player.
ProductionPlan plan_for_state(const GameState& state, int player, const Hyperparameters& hp);

}  // namespace kag
