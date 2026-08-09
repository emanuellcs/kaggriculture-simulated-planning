#pragma once

#include <algorithm>
#include <cmath>

#include "board.hpp"

namespace kag {
namespace detail {

// Farmer/hand action opcodes.
enum Op : uint8_t {
    OP_PASS = 0,
    OP_NORTH, OP_SOUTH, OP_EAST, OP_WEST,
    OP_PICKUP, OP_DROP, OP_PLANT, OP_WATER, OP_HARVEST, OP_FERTILIZE,
    OP_BUILD_COOP, OP_BUILD_PASTURE, OP_DIG, OP_PLACE, OP_FEED,
    OP_COLLECT_FERTILIZER, OP_CARE,
};

inline double shape(PriceFunc f, double x) {
    x = std::max(0.0, x);
    switch (f) {
        case F_SQ: return x * x;
        case F_SQRT: return std::sqrt(x);
        case F_LOG: return std::log(1.0 + x);
        case F_LOG10: return std::log10(1.0 + x);
        case F_LINEAR:
        default: return x;
    }
}

inline int market_price(Product item, int inventory) {
    const MarketParams& p = MARKET_PARAMS[static_cast<size_t>(item)];
    const double base = p.base;
    const int I0 = p.I0;
    const int T = p.T;
    double price;
    if (inventory < I0) {
        const double amp = p.below_target * base / shape(p.below_func, static_cast<double>(T));
        price = base + amp * shape(p.below_func, static_cast<double>(I0 - inventory));
    } else {
        const double amp = p.above_target * base / shape(p.above_func, static_cast<double>(T));
        price = base - amp * shape(p.above_func, static_cast<double>(inventory - I0));
    }
    return std::max(PRICE_FLOOR, static_cast<int>(std::lround(price)));
}

inline int fib(int n) {
    int a = 1, b = 1;
    for (int i = 0; i < n; ++i) {
        int next = a + b;
        a = b;
        b = next;
    }
    return a;
}

inline bool is_shed_adjacent(int x, int y, int board_size) {
    const int half = board_size / 2;
    return (x == half - 1 && y == half - 1) || (x == half && y == half - 1) ||
           (x == half - 1 && y == half) || (x == half && y == half);
}

// Simple deterministic xorshift64 for weeds/shops in simulation and planning.
struct Rng {
    uint64_t state = 0x9e3779b97f4a7c15ULL;
    explicit Rng(uint64_t seed) : state(seed ? seed : 1ULL) {}
    uint64_t next() {
        uint64_t x = state;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        state = x;
        return x;
    }
    double unit() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }
};

inline int quadrant_index(int x, int y, int board_size) {
    const int half = board_size / 2;
    int q = 0;
    if (y >= half) q |= 2;   // S
    if (x >= half) q |= 1;   // E
    return q;                // 0=NW,1=NE,2=SW,3=SE
}

inline int animal_product_index(int animal) {
    return ANIMALS[static_cast<size_t>(animal)].product;
}

}  // namespace detail

// Simulator entry points (implemented in kaggriculture_sim.cpp).
void apply_turn(GameState& gs, const ActionInput& a0, const ActionInput& a1, detail::Rng& rng);
void do_hire(GameState& gs, int player);
void do_buy_land(GameState& gs, int player);

}  // namespace kag
