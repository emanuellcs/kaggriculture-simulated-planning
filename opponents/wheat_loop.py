"""Wheat-bulk archetype: dense wheat planting, aggressive hiring, bulk selling."""

from __future__ import annotations

from .baseline import agent as _wheat_agent


def agent(obs, config=None):
    actions = _wheat_agent(obs, config)
    me = None
    if isinstance(obs, dict):
        me = obs["farms"][obs["player"]]
    else:
        me = obs.farms[obs.player]
    market = actions["market"]
    # Hire up to four hands when affordable.
    if len(me["hands"]) < 4 and me["money"] >= 400:
        for _ in range(2):
            market.insert(0, ["HIRE"])
    return {"farmer": actions["farmer"], "hands": actions["hands"], "market": market[:10]}


def act(obs, config=None):
    try:
        return agent(obs, config)
    except Exception:
        return {"farmer": ["PASS"], "hands": [], "market": []}
