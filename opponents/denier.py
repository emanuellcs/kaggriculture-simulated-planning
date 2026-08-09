"""Denier archetype: sells everything immediately to drive prices to the floor.

Deliberately dumps the full shed every turn, pressuring the shared market
price down. Tests the champion's resilience to opponent price manipulation.
"""

from __future__ import annotations

from ._kag_common import _get, nearest_empty, nearest_tile, path_first


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
        if n > 0 and item not in ("GOOSE", "COW", "SHEEP"):
            market.append(["SELL", item, n])
    if seeds.get("WHEAT", 0) == 0 and me["money"] >= 10:
        market.append(["BUY_SEED", "WHEAT", 2])

    farmer = ["PASS"]
    tile = me["tiles"][fy][fx]
    if isinstance(tile, dict) and tile.get("kind") == "PLANT":
        if tile["yield_units"] > 0 and int(_get(obs, "day", 0)) - tile["planted_day"] >= 2:
            farmer = ["HARVEST"]
        elif not tile["watered_today"]:
            farmer = ["WATER"]
    elif tile is None and seeds.get("WHEAT", 0) > 0:
        farmer = ["PLANT", "WHEAT"]
    else:
        empty = nearest_empty(obs, player, fx, fy)
        if empty is not None:
            step = path_first(obs, player, fx, fy, empty[0], empty[1])
            if step:
                farmer = [step]
    return {"farmer": farmer, "hands": [["PASS"]] * len(me["hands"]), "market": market[:10]}


def act(obs, config=None):
    try:
        return agent(obs, config)
    except Exception:
        return {"farmer": ["PASS"], "hands": [], "market": []}
