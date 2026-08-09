"""Benchmark harness: measure the champion against the league.

Plays the champion against every opponent in ``config/league.json`` plus the
built-in starter/random policies on held-out seeds with seat rotation, and
reports win rates with a Wilson confidence interval.
"""

from __future__ import annotations

import argparse
import logging
import math
import multiprocessing
import sys
from pathlib import Path

multiprocessing.set_start_method("spawn", force=True)

_ROOT = Path(__file__).resolve().parents[1]
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

import main  # noqa: E402
from tuning import evaluation, league as league_mod, reporting, schema  # noqa: E402

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
_LOG = logging.getLogger("bench")

BUILTIN_OPPONENTS = ("starter", "random", "pass")


def _resolve_agent(kind, name, params):
    if kind == "builtin":
        return name
    if kind == "handcrafted":
        module = __import__(f"opponents.{name}", fromlist=["agent"])
        return module.agent

    def checkpoint_agent(obs, config=None):
        main.set_hyperparameters(**params)
        return main.agent(obs, config)

    return checkpoint_agent


def _outcome(seed, seat, opponent, config):
    from kaggle_environments import make

    env = make("kaggriculture", configuration={"episodeSteps": config["steps"]}, debug=False)
    agents = [opponent, opponent]
    agents[seat] = main.agent
    result = evaluation.run_with_timeout(lambda: env.run(agents), config["timeout"])
    if result is None:
        return 0.0
    last = result[-1]
    if evaluation.state_failed(last[seat].status):
        return 0.0
    my = float(last[seat].reward)
    opp = float(last[1 - seat].reward)
    return 1.0 if my > opp else (0.5 if my == opp else 0.0)


def wilson_ci(wins, games, z=1.96):
    if games == 0:
        return 0.0, 0.0, 0.0
    p = wins / games
    denom = 1 + z * z / games
    center = (p + z * z / (2 * games)) / denom
    margin = z * math.sqrt((p * (1 - p) + z * z / (4 * games)) / games) / denom
    return p, max(0.0, center - margin), min(1.0, center + margin)


def main_cli(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seeds", type=int, default=20)
    parser.add_argument("--base-seed", type=int, default=9000)
    parser.add_argument("--steps", type=int, default=720)
    parser.add_argument("--timeout", type=float, default=30.0)
    args = parser.parse_args(argv)

    hyper_config = _ROOT / "config" / "hyperparameters.json"
    league_path = _ROOT / "config" / "league.json"
    champion = dict(schema.Schema.load(hyper_config).defaults())
    champion.update(reporting.read_champion(hyper_config))
    main.set_hyperparameters(**champion)

    lg = league_mod.League(league_path)
    opponents = [("builtin", n, None) for n in BUILTIN_OPPONENTS]
    opponents += [("handcrafted", e["name"], None) for e in lg.entries
                  if e["name"] not in BUILTIN_OPPONENTS]
    opponents += [("checkpoint", e["name"], e.get("params", {})) for e in lg.entries
                  if e.get("kind") == "checkpoint"]

    config = {"steps": args.steps, "timeout": args.timeout}
    for kind, name, params in opponents:
        opponent = _resolve_agent(kind, name, params)
        wins = games = 0
        for i in range(args.seeds):
            for seat in (0, 1):
                wins += _outcome(args.base_seed + i, seat, opponent, config)
                games += 1
        p, lo, hi = wilson_ci(wins, games)
        print(f"vs {name:<24} {games:4d} games  WR={p:.3f}  CI=[{lo:.3f},{hi:.3f}]")
    return 0


if __name__ == "__main__":
    raise SystemExit(main_cli())
