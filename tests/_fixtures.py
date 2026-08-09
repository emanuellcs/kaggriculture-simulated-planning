"""Shared fixtures for the Kaggriculture test modules."""

from __future__ import annotations

from pathlib import Path

import main

REPO_ROOT = Path(__file__).resolve().parents[1]


def load_engine():
    """Ensure the native extension is available and return the module."""
    assert main._ensure_native_engine()
    return main.kaggriculture_engine


def fresh_obs():
    """A compact starting observation for the native engine."""
    tiles = [[None if (x < 5 and y < 5) else "LOCKED" for x in range(10)] for y in range(10)]
    return {
        "player": 0,
        "step": 0,
        "day": 0,
        "hour": 0,
        "farms": [
            {"money": 3000.0, "tiles": tiles, "farmer": [4, 4], "hands": [],
             "unlocked_quadrants": ["NW"], "hires_today": 0},
            {"money": 3000.0, "tiles": tiles, "farmer": [4, 4], "hands": [],
             "unlocked_quadrants": ["NW"], "hires_today": 0},
        ],
        "market": {
            "inventory": {k: 10000 for k in (
                "WHEAT", "CARROT", "TOMATO", "STRAWBERRY", "MELON", "EGG", "MILK", "WOOL", "FERTILIZER")},
            "prices": {"WHEAT": 25, "CARROT": 35, "TOMATO": 60, "STRAWBERRY": 120, "MELON": 250,
                       "EGG": 50, "MILK": 160, "WOOL": 200, "FERTILIZER": 100},
        },
        "town": {"unlocked_shops": []},
        "private": {"shed": {}, "seeds": {}, "inventories": [{}]},
    }
