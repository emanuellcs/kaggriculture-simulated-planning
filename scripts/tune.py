"""Optuna tuning for the Kaggriculture simulated-planning engine.

Each trial samples the production-plan and market-policy hyperparameters,
injects them via ``main.set_hyperparameters``, and scores the candidate against
a fixed opponent pool (built-in starter, hand-crafted archetypes, self-play
checkpoints) over fixed seeds with seat rotation. On success the champion is
written back to ``config/hyperparameters.json``.
"""

from __future__ import annotations

import argparse
import functools
import logging
import multiprocessing
import sys
import traceback
from dataclasses import dataclass
from pathlib import Path

multiprocessing.set_start_method("spawn", force=True)

import optuna  # noqa: E402

_ROOT = Path(__file__).resolve().parents[1]
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

import main  # noqa: E402
from tuning import evaluation, league as league_mod, optuna_runner, reporting, schema  # noqa: E402

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
_LOG = logging.getLogger("tune")

BUILTIN_OPPONENTS = ("starter", "random", "pass")
FAILURE_SCORE = evaluation.FAILURE_SCORE


@dataclass(frozen=True)
class EvalConfig:
    seeds: int
    base_seed: int
    steps: int
    timeout: float
    opponent_pool: tuple


def _resolve_opponent(kind, name, params):
    if kind == "builtin":
        return name
    if kind == "handcrafted":
        module = __import__(f"opponents.{name}", fromlist=["agent"])
        return module.agent

    def checkpoint_agent(obs, config=None):
        main.set_hyperparameters(**params)
        return main.agent(obs, config)

    return checkpoint_agent


def _play(seed, seat, opponent, config: EvalConfig):
    from kaggle_environments import make

    env = make("kaggriculture", configuration={"episodeSteps": config.steps}, debug=False)
    agents = [opponent, opponent]
    agents[seat] = main.agent
    steps = evaluation.run_with_timeout(lambda: env.run(agents), config.timeout)
    if steps is None:
        return 0.0
    last = steps[-1]
    if evaluation.state_failed(last[seat].status):
        return 0.0
    my = float(last[seat].reward)
    opp = float(last[1 - seat].reward)
    return 1.0 if my > opp else (0.5 if my == opp else 0.0)


def evaluate_hp(hp, config: EvalConfig) -> float:
    try:
        main.set_hyperparameters(**hp)
        total = 0.0
        games = 0
        for kind, name, params in config.opponent_pool:
            opponent = _resolve_opponent(kind, name, params)
            main.set_hyperparameters(**hp)
            for i in range(config.seeds):
                for seat in (0, 1):
                    total += _play(config.base_seed + i, seat, opponent, config)
                    games += 1
        rate = total / max(1, games)
        _LOG.info("hp rate=%.3f games=%d", rate, games)
        return rate
    except Exception:  # noqa: BLE001
        _LOG.error("evaluation failed:\n%s", traceback.format_exc())
        return FAILURE_SCORE


def build_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trials", type=int, default=200)
    parser.add_argument("--n-jobs", type=int, default=8)
    parser.add_argument("--seeds", type=int, default=3)
    parser.add_argument("--steps", type=int, default=240, help="Episode step cap for matches")
    parser.add_argument("--time-budget", type=int, default=3600)
    parser.add_argument("--storage", default="sqlite:///tune.db")
    parser.add_argument("--study-name", default="kaggriculture-vs-league")
    parser.add_argument("--base-seed", type=int, default=42)
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--promote", action="store_true")
    return parser


def main_cli(argv=None) -> int:
    args = build_parser().parse_args(argv)
    hyper_config = _ROOT / "config" / "hyperparameters.json"
    league_path = _ROOT / "config" / "league.json"
    sch = schema.Schema.load(hyper_config)

    lg = league_mod.League(league_path)
    pool = [("builtin", n, None) for n in BUILTIN_OPPONENTS]
    pool += [("handcrafted", e["name"], None) for e in lg.entries
             if e["name"] not in BUILTIN_OPPONENTS]
    pool += [("checkpoint", e["name"], e.get("params", {})) for e in lg.entries
             if e.get("kind") == "checkpoint"]

    config = EvalConfig(seeds=args.seeds, base_seed=args.base_seed, steps=args.steps,
                        timeout=args.timeout, opponent_pool=tuple(pool))

    champion = dict(sch.defaults())
    champion.update(reporting.read_champion(hyper_config))

    best = optuna_runner.run_study(
        objective=functools.partial(evaluate_hp, config=config),
        sample=sch.sample_trial,
        trials=args.trials,
        n_jobs=args.n_jobs,
        storage=args.storage,
        study_name=args.study_name,
        time_budget_s=args.time_budget,
        champion=champion,
        seed_offset=args.base_seed,
    )
    if best is None:
        print("No successful trials; nothing to promote.")
        return 1
    if args.promote:
        reporting.promote_champion(hyper_config, league_path, "champion", best,
                                   note=f"promoted from {args.study_name}")
        print(f"Promoted champion -> {hyper_config}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main_cli())
