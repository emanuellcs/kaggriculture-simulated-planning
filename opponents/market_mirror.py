"""Market-mirror archetype: mirrors the opponent's crop mix to crowd the market.

By planting the same crops as the opponent, this policy competes directly on
the same products, testing the champion's ability to manage price pressure.
"""

from __future__ import annotations

from ._kag_common import CROPS, SEED_COST, _get, nearest_empty, nearest_tile, path_first


def _opponent_mix(obs, player):
    farms = _get(obs, "farms")
    opp = farms[1 - player]
    counts = {c: 0 for c in CROPS}
    for row in opp["tiles"]:
        for t in row:
            if isinstance(t, dict) and t.get("kind") == "PLANT":
                counts[t["crop"]] = counts.get(t["crop"], 0) + 1
    return counts


def agent(obs, config=None):
    del config
    player = int(_get(obs, "player", 0))
    me = _get(obs, "farms")[player]
    private = _get(obs, "private") or {}
    fx, fy = me["farmer"]
    seeds = private.get("seeds", {})
    shed = private.get("shed", {})
    market = []
    for item, n in shed.items():
        if n > 0 and item != "GOOSE" and item != "COW" and item != "SHEEP":
            market.append(["SELL", item, n])
    mix = _opponent_mix(obs, player)
    top = max(CROPS, key=lambda c: mix[c]) if max(mix.values()) > 0 else "WHEAT"
    if seeds.get(top, 0) == 0 and me["money"] >= SEED_COST[top]:
        market.append(["BUY_SEED", top, 3])

    farmer = ["PASS"]
    tile = me["tiles"][fy][fx]
    if isinstance(tile, dict) and tile.get("kind") == "PLANT":
        if tile["yield_units"] > 0 and int(_get(obs, "day", 0)) - tile["planted_day"] >= 2:
            farmer = ["HARVEST"]
        elif not tile["watered_today"]:
            farmer = ["WATER"]
    elif tile is None and seeds.get(top, 0) > 0:
        farmer = ["PLANT", top]
    else:
        empty = nearest_empty(obs, player, fx, fy)
        if empty is not None:
            step = path_first(obs, player, fx, fy, empty[0], empty[1])
            if step:
                farmer = [step]
    return {"farmer": farmer, "hands": [["PASS"]] * len(me["hands"]), "market": market}


def act(obs, config=None):
    try:
        return agent(obs, config)
    except Exception:
        return {"farmer": ["PASS"], "hands": [], "market": []}
