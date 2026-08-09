"""Pybind bridge and market-price tests."""

from __future__ import annotations

import pytest

import main
from _fixtures import fresh_obs, load_engine


def test_native_engine_available():
    """The native extension must import and expose the Engine."""
    module = load_engine()
    assert hasattr(module, "Engine")
    assert hasattr(module, "market_price")


def test_observation_parses_and_returns_action_dict():
    """A fresh observation must produce a valid Kaggriculture action dict."""
    engine = load_engine().Engine()
    engine.update_observation(fresh_obs(), None)
    actions = engine.choose_actions(900)
    assert isinstance(actions, dict)
    assert isinstance(actions["farmer"], list)
    assert isinstance(actions["hands"], list)
    assert isinstance(actions["market"], list)
    assert actions["farmer"][0] in {"NORTH", "SOUTH", "EAST", "WEST", "PASS", "PLANT",
                                    "WATER", "HARVEST", "BUILD_COOP", "BUILD_PASTURE", "DIG"}


def test_market_price_anchors():
    """The native price function matches the published curve anchors."""
    mp = load_engine().market_price
    # Base prices at I0.
    assert mp("WHEAT", 10000) == 25
    assert mp("MELON", 10000) == 250
    assert mp("STRAWBERRY", 10000) == 120
    # Premium goods crash hard on glut.
    assert mp("MELON", 10000 + 300) == 1
    assert mp("WOOL", 10000 + 105) == 1
    # Wheat absorbs gluts.
    assert mp("WHEAT", 10000 + 800) == 19
    # Scarcity raises prices.
    assert mp("TOMATO", 10000 - 200) == 84
    # Price floor.
    assert mp("CARROT", 10000 + 900) == 1
    assert mp("WHEAT", 0) > 25


def test_hyperparameters_roundtrip():
    """set_hyperparameters and get_hyperparameters must round-trip."""
    module = load_engine()
    engine = module.Engine()
    engine.set_hyperparameters({"target_cows": 3, "wheat_tiles": 12, "max_hands_per_day": 5})
    hp = engine.get_hyperparameters()
    assert hp["target_cows"] == 3
    assert hp["wheat_tiles"] == 12
    assert hp["max_hands_per_day"] == 5
    assert hp["target_sheep"] == 0


def test_main_agent_returns_action_dict():
    """main.agent must return the correct top-level action shape."""
    actions = main.agent(fresh_obs())
    assert set(actions) == {"farmer", "hands", "market"}
    assert isinstance(actions["market"], list)
    for order in actions["market"]:
        assert isinstance(order, list) and len(order) >= 1
