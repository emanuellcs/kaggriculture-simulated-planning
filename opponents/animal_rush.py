"""Animal-rush archetype: builds coops, buys geese, feeds, collects eggs."""

from __future__ import annotations

from ._kag_common import _get, nearest_tile, path_first


def agent(obs, config=None):
    del config
    player = int(_get(obs, "player", 0))
    me = _get(obs, "farms")[player]
    private = _get(obs, "private") or {}
    fx, fy = me["farmer"]
    shed = private.get("shed", {})
    market = []
    if shed.get("EGG", 0) > 0:
        market.append(["SELL", "EGG", shed["EGG"]])
    # Buy a goose once a coop is ready.
    has_coop = any(isinstance(t, dict) and t.get("kind") == "COOP" for row in me["tiles"] for t in row)
    if has_coop and shed.get("GOOSE", 0) == 0 and me["money"] >= 400:
        market.append(["BUY_ANIMAL", "GOOSE", 1])

    farmer = ["PASS"]
    tile = me["tiles"][fy][fx]
    if isinstance(tile, dict) and tile.get("kind") == "COOP":
        if tile.get("animal"):
            if not tile.get("fed_today") and shed.get("WHEAT", 0) > 0:
                farmer = ["FEED"]
            elif tile.get("yield_units", 0) > 0:
                farmer = ["HARVEST"]
            elif tile.get("fertilizer_available"):
                farmer = ["COLLECT_FERTILIZER"]
            elif not tile.get("cared_today"):
                farmer = ["CARE"]
        else:
            farmer = ["PASS"]
    elif isinstance(tile, dict) and tile.get("kind") == "PLANT":
        if not tile.get("watered_today"):
            farmer = ["WATER"]
        elif tile.get("yield_units", 0) > 0:
            farmer = ["HARVEST"]
    elif tile is None and shed.get("GOOSE", 0) > 0:
        # Need a coop first.
        target = nearest_tile(obs, player, fx, fy, lambda t: t.get("kind") == "COOP")
        if target is not None:
            step = path_first(obs, player, fx, fy, target[0], target[1])
            if step:
                farmer = [step]
        elif me["money"] > 100:
            farmer = ["BUILD_COOP"]
    else:
        target = nearest_tile(obs, player, fx, fy, lambda t: t.get("kind") == "COOP")
        if target is not None:
            step = path_first(obs, player, fx, fy, target[0], target[1])
            if step:
                farmer = [step]
    return {"farmer": farmer, "hands": [["PASS"]] * len(me["hands"]), "market": market}


def act(obs, config=None):
    try:
        return agent(obs, config)
    except Exception:
        return {"farmer": ["PASS"], "hands": [], "market": []}
