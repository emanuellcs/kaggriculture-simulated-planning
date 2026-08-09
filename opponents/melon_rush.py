"""Melon-rush archetype: saves cash early, plants melons, sells timed."""

from __future__ import annotations

from ._kag_common import SEED_COST, _get, nearest_empty, nearest_tile, path_first

CROP = "MELON"


def agent(obs, config=None):
    del config
    player = int(_get(obs, "player", 0))
    me = _get(obs, "farms")[player]
    private = _get(obs, "private") or {}
    fx, fy = me["farmer"]
    day = int(_get(obs, "day", 0))
    seeds = private.get("seeds", {})
    shed = private.get("shed", {})
    market = []
    for item in ("MELON", "WHEAT"):
        if shed.get(item, 0) > 0:
            market.append(["SELL", item, shed[item]])
    # Plant melons early; they take 10 days to yield.
    if seeds.get("MELON", 0) == 0 and me["money"] >= SEED_COST["MELON"] and day < 15:
        market.append(["BUY_SEED", "MELON", 4])

    farmer = ["PASS"]
    tile = me["tiles"][fy][fx]
    if isinstance(tile, dict) and tile.get("kind") == "PLANT" and tile.get("crop") == CROP:
        if tile["yield_units"] > 0 and day - tile["planted_day"] >= 10:
            farmer = ["HARVEST"]
        elif not tile["watered_today"]:
            farmer = ["WATER"]
    elif tile is None and seeds.get("MELON", 0) > 0:
        farmer = ["PLANT", CROP]
    else:
        target = nearest_tile(obs, player, fx, fy, lambda t: t.get("kind") == "PLANT" and t.get("crop") == CROP and not t.get("watered_today"))
        if target is not None:
            step = path_first(obs, player, fx, fy, target[0], target[1])
            if step:
                farmer = [step]
        if farmer == ["PASS"]:
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
