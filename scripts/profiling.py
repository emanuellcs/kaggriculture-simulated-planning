"""Per-turn latency profiling for the Kaggriculture agent.

Runs the native agent on a compact observation for ``--calls`` invocations and
reports the latency distribution (p50/p95/max). ``--max-p95`` turns the p95
into a hard exit-code gate for CI.

Run:
    PYTHONPATH=build python scripts/profiling.py --calls 50
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

_ROOT = Path(__file__).resolve().parents[1]
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

import main  # noqa: E402


def _fixture():
    return {
        "player": 0, "step": 0, "day": 0, "hour": 0,
        "farms": [
            {"money": 3000.0,
             "tiles": [[None if (x < 5 and y < 5) else "LOCKED" for x in range(10)] for y in range(10)],
             "farmer": [4, 4], "hands": [], "unlocked_quadrants": ["NW"], "hires_today": 0},
            {"money": 3000.0,
             "tiles": [[None if (x < 5 and y < 5) else "LOCKED" for x in range(10)] for y in range(10)],
             "farmer": [4, 4], "hands": [], "unlocked_quadrants": ["NW"], "hires_today": 0},
        ],
        "market": {"inventory": {k: 10000 for k in (
            "WHEAT", "CARROT", "TOMATO", "STRAWBERRY", "MELON", "EGG", "MILK", "WOOL", "FERTILIZER")},
                   "prices": {"WHEAT": 25, "CARROT": 35, "TOMATO": 60, "STRAWBERRY": 120, "MELON": 250,
                              "EGG": 50, "MILK": 160, "WOOL": 200, "FERTILIZER": 100}},
        "town": {"unlocked_shops": []},
        "private": {"shed": {}, "seeds": {}, "inventories": [{}]},
    }


def main_cli(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--calls", type=int, default=50)
    parser.add_argument("--max-p95", type=float, default=0.0, help="Fail if p95 exceeds this (ms)")
    args = parser.parse_args(argv)

    obs = _fixture()
    latencies = []
    for _ in range(args.calls):
        start = time.perf_counter()
        actions = main.agent(obs)
        latencies.append((time.perf_counter() - start) * 1000.0)
        assert isinstance(actions, dict) and "farmer" in actions and "market" in actions

    latencies.sort()
    p50 = latencies[len(latencies) // 2]
    p95 = latencies[int(len(latencies) * 0.95) - 1]
    peak = latencies[-1]
    print(f"calls={args.calls} p50={p50:.2f}ms p95={p95:.2f}ms max={peak:.2f}ms")

    if args.max_p95 > 0 and p95 > args.max_p95:
        print(f"perf gate failed: p95={p95:.2f}ms > {args.max_p95}ms")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main_cli())
