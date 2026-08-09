"""Market model tests: price curves, floor, and shape functions."""

from __future__ import annotations

import math

import pytest

from _fixtures import load_engine


def test_shape_functions():
    """The shape functions must match the documented definitions."""
    shape = load_engine().shape_value
    assert shape("linear", 5.0) == pytest.approx(5.0)
    assert shape("sq", 3.0) == pytest.approx(9.0)
    assert shape("sqrt", 4.0) == pytest.approx(2.0)
    assert shape("log", 0.0) == pytest.approx(0.0)
    assert shape("log", 5.0) == pytest.approx(math.log(6.0))
    assert shape("log10", 9.0) == pytest.approx(math.log10(10.0))


def test_price_curve_symmetry():
    """Moving T units past I0 shifts the price by roughly target*base."""
    mp = load_engine().market_price
    # Tomato: above linear 0.60, base 60, T 200 -> about $24 at +200.
    assert 20 <= mp("TOMATO", 10000 + 200) <= 24
    # Wheat: above log 0.20, base 25 -> ~$20 at +400.
    assert 18 <= mp("WHEAT", 10000 + 400) <= 21


def test_premium_glut_crashes_to_floor():
    """Premium goods hit the $1 floor as inventory grows."""
    mp = load_engine().market_price
    assert mp("STRAWBERRY", 10000 + 100) == 1
    assert mp("MELON", 10000 + 300) == 1
    assert mp("MILK", 10000 + 122) == 1
    assert mp("WOOL", 10000 + 105) == 1


def test_staples_absorb_glut():
    """Staples retain value even after large gluts."""
    mp = load_engine().market_price
    assert mp("WHEAT", 10000 + 800) == 19
    assert mp("EGG", 10000 + 664) == 39


def test_scarcity_raises_price():
    """Inventory below I0 raises the price above base."""
    mp = load_engine().market_price
    assert mp("CARROT", 10000 - 450) == 42
    assert mp("TOMATO", 10000 - 200) == 84


def test_price_floor_respected():
    """Prices never fall below $1."""
    mp = load_engine().market_price
    for item in ("WHEAT", "CARROT", "TOMATO", "STRAWBERRY", "MELON", "EGG", "MILK", "WOOL", "FERTILIZER"):
        assert mp(item, 10000 + 5000) >= 1
