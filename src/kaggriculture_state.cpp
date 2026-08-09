#include "kaggriculture_internal.hpp"

#include "board.hpp"
#include "constants.hpp"

namespace kag {

void reset_game(GameState& gs, const GameConfig& cfg) {
    gs = GameState{};
    gs.config = cfg;
    gs.market.inventory.fill(MARKET_I0);
    for (int i = 0; i < P_COUNT; ++i)
        gs.market.prices[static_cast<size_t>(i)] =
            detail::market_price(static_cast<Product>(i), MARKET_I0);
    const int half = cfg.board_size / 2;
    for (int p = 0; p < 2; ++p) {
        Farm& farm = gs.farms[static_cast<size_t>(p)];
        farm.money = static_cast<double>(cfg.starting_money);
        farm.board_size = cfg.board_size;
        farm.unlocked_quadrants = 0x1;  // NW
        for (int y = 0; y < cfg.board_size; ++y)
            for (int x = 0; x < cfg.board_size; ++x)
                farm.tiles[static_cast<size_t>(y)][static_cast<size_t>(x)].kind =
                    detail::quadrant_index(x, y, cfg.board_size) == 0 ? TK_EMPTY : TK_LOCKED;
        farm.farmer_x = half - 1;
        farm.farmer_y = half - 1;
    }
    gs.step = 0;
    gs.day = 0;
    gs.hour = 0;
    gs.done = false;
}

}  // namespace kag
