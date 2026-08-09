#pragma once

#include <array>

#include "board.hpp"
#include "constants.hpp"

namespace kag {

// Parsed per-player public farm observation.
struct FarmObservation {
    double money = 0.0;
    std::array<std::array<uint8_t, 16>, 16> tile_kind{};
    std::array<std::array<uint8_t, 16>, 16> tile_crop{};
    std::array<std::array<int16_t, 16>, 16> planted_day{};
    std::array<std::array<int16_t, 16>, 16> yield_units{};
    std::array<std::array<uint8_t, 16>, 16> watered_today{};
    std::array<std::array<uint8_t, 16>, 16> consec_unwatered{};
    std::array<std::array<int16_t, 16>, 16> fert_until{};
    std::array<std::array<uint8_t, 16>, 16> animal{};
    std::array<std::array<int16_t, 16>, 16> placed_day{};
    std::array<std::array<uint8_t, 16>, 16> fed_today{};
    std::array<std::array<uint8_t, 16>, 16> consec_unfed{};
    std::array<std::array<uint8_t, 16>, 16> cared_today{};
    std::array<std::array<uint8_t, 16>, 16> fert_available{};
    std::array<std::array<int16_t, 16>, 16> pending_care{};
    int farmer_x = 0;
    int farmer_y = 0;
    int hand_count = 0;
    std::array<std::array<int16_t, 2>, MAX_HANDS> hands{};
    uint8_t unlocked_quadrants = 0x1;
    int hires_today = 0;
};

// Complete decoded observation passed to the native engine.
struct ObservationInput {
    int player = 0;
    int step = 0;
    int day = 0;
    int hour = 0;
    std::array<FarmObservation, 2> farms{};
    PrivateState private_state{};
    MarketState market{};
    TownState town{};
    GameConfig config{};
};

}  // namespace kag
