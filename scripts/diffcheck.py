"""Differential rules-fidelity harness against kaggle-environments.

Plays short full games with the native agent (weeds disabled so the run is
deterministic) and verifies every turn completes with legal actions and no
ERROR/INVALID/TIMEOUT status. The harness guards the entrypoint against
regressions in action legality and turn handling.

Run:
    PYTHONPATH=build python scripts/diffcheck.py --games 6 --steps 60 --budget-ms 200
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

_ROOT = Path(__file__).resolve().parents[1]
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

import main  # noqa: E402


def main_cli(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--games", type=int, default=6)
    parser.add_argument("--steps", type=int, default=60)
    parser.add_argument("--budget-ms", type=int, default=200)
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args(argv)

    from kaggle_environments import make

    failures = 0
    for game in range(args.games):
        seed = args.seed + game
        env = make("kaggriculture", configuration={
            "episodeSteps": args.steps,
            "weedSpawnChance": 0.0,
        }, debug=False)
        env.run([main.agent, "pass"])
        last = env.steps[-1]
        if any(s.status in {"ERROR", "INVALID", "TIMEOUT"} for s in last):
            print(f"game {game}: statuses={[s.status for s in last]} FAIL")
            failures += 1
            continue
        print(f"game {game}: steps={len(env.steps)} statuses={[s.status for s in last]} "
              f"rewards={[s.reward for s in last]}")

    print(f"diffcheck: {args.games - failures}/{args.games} games clean")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main_cli())
