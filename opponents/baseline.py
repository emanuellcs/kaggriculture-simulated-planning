"""Wheat-focused benchmark opponent with BFS movement.

Plants wheat on unlocked empty tiles near the shed, waters daily, harvests at
peak, sells from the shed, and hires a couple of hands when affordable. A
deterministic, reasonable baseline for tuning and benchmarking.
"""

from __future__ import annotations

from ._kag_common import (
    CROPS,
    SEED_COST,
    _get,
    nearest_empty,
    nearest_tile,
    path_first,
)


def agent(obs, config=None):
    del config
    player = int(_get(obs, "player", 0))
    me = _get(obs, "farms")[player]
    private = _get(obs, "private") or {}
    fx, fy = me["farmer"]
    bs = len(me["tiles"])
    day = int(_get(obs, "day", 0))
    seeds = private.get("seeds", {})
    shed = private.get("shed", {})

    market = []
    for item in ("WHEAT", "CARROT"):
        if shed.get(item, 0) > 0:
            market.append(["SELL", item, shed[item]])
    if seeds.get("WHEAT", 0) == 0 and me["money"] >= SEED_COST["WHEAT"]:
        market.append(["BUY_SEED", "WHEAT", 3])
    if len(me["hands"]) < 2 and me["money"] >= 200:
        market.append(["HIRE"])

    farmer = ["PASS"]
    tile = me["tiles"][fy][fx]

    # Water or harvest an existing wheat plant at the farmer's tile.
    if isinstance(tile, dict) and tile.get("kind") == "PLANT" and tile.get("crop") == "WHEAT":
        if tile["yield_units"] > 0 and day - tile["planted_day"] >= 2:
            farmer = ["HARVEST"]
        elif not tile["watered_today"]:
            farmer = ["WATER"]
    elif tile is None and seeds.get("WHEAT", 0) > 0:
        farmer = ["PLANT", "WHEAT"]
    else:
        # Move toward an unwatered/ready wheat plant, or an empty tile to plant.
        target = nearest_tile(obs, player, fx, fy, lambda t: t.get("kind") == "PLANT" and t.get("crop") == "WHEAT")
        if target is not None:
            tx, ty = target
            t = me["tiles"][ty][tx]
            if t["yield_units"] > 0 and day - t["planted_day"] >= 2:
                pass  # harvest soon
            if not t["watered_today"]:
                step = path_first(obs, player, fx, fy, tx, ty)
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
