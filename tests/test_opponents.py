"""Opponent archetype registration tests."""

from __future__ import annotations

import pytest


@pytest.mark.parametrize(
    "name",
    ["baseline", "wheat_loop", "melon_rush", "animal_rush", "market_mirror", "denier"],
)
def test_archetype_imports_and_returns_action_dict(name):
    """Every archetype must expose a callable returning the action shape."""

    module = __import__(f"opponents.{name}", fromlist=["agent"])
    assert callable(module.agent)

    obs = {
        "player": 0, "step": 0, "day": 0, "hour": 0,
        "farms": [
            {"money": 3000.0, "tiles": [[None] * 10 for _ in range(10)],
             "farmer": [4, 4], "hands": [], "unlocked_quadrants": ["NW"], "hires_today": 0},
            {"money": 3000.0, "tiles": [[None] * 10 for _ in range(10)],
             "farmer": [4, 4], "hands": [], "unlocked_quadrants": ["NW"], "hires_today": 0},
        ],
        "market": {"inventory": {}, "prices": {}},
        "town": {"unlocked_shops": []},
        "private": {"shed": {}, "seeds": {}, "inventories": [{}]},
    }
    actions = module.agent(obs)
    assert set(actions) == {"farmer", "hands", "market"}
    assert isinstance(actions["market"], list)
