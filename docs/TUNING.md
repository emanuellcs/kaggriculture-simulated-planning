# Tuning System

This document describes the tuning pipeline for the Kaggriculture simulated-planning engine: the hyperparameter schema, the Optuna runner, the evaluation protocol, and the opponent league.

## Overview

Tuning optimizes the production-plan and market-policy hyperparameters against a league of opponents. The system follows the same design as the other repositories: a single source of truth in `config/hyperparameters.json`, fixed seeds and full seat rotation for a reliable signal, and champion write-back on promotion.

The harness is `scripts/tune.py`. It uses the reusable package under `tuning/`.

## Single Source of Truth

`config/hyperparameters.json` holds the `champion` block and the `schema` block. The schema defines every tunable with its type, bounds, and default; `tuning/schema.py` maps those definitions onto Optuna `Trial` suggestions.

### Parameter Groups

| Group | Parameters |
| --- | --- |
| animals | `target_cows`, `target_sheep`, `target_geese` |
| crops | `wheat_tiles`, `carrot_tiles`, `melon_tiles`, `tomato_tiles`, `strawberry_tiles` |
| land | `buy_land_ne`, `buy_land_sw`, `buy_land_se` |
| labor | `max_hands_per_day` |
| market | `shed_keep`, `tranche_after_town`, `wheat_feed_reserve`, `deny_threshold` |

Numeric values are not duplicated in documentation. See `config/hyperparameters.json` for the authoritative values.

## Optuna Runner

`tuning/optuna_runner.py` implements the ask-and-tell loop with a multivariate TPE sampler, spawned worker processes, incumbent enqueue, SQLite resumption, a wall-clock deadline, and graceful handling of worker crashes and timeouts.

## Evaluation Protocol

Each trial scores one hyperparameter set in a worker process:

- A fixed seed set is shared across all trials.
- Every match rotates the candidate through both seats.
- The opponent pool is fixed for the study: built-in `starter`/`random`/`pass`, the hand-crafted archetypes, and self-play checkpoints.
- The trial score is the seat-rotated win rate (draws score half a win).

## Opponent League

`config/league.json` records named opponents with Elo ratings. Hand-crafted archetypes cover distinct strategies (wheat bulk, melon timing, animal focus, market mirroring, price denial); self-play checkpoints are added when a champion is promoted.

## Champion Promotion

`scripts/tune.py --promote` writes the best parameters into the `champion` block of `config/hyperparameters.json` and registers the champion as a self-play checkpoint in `config/league.json`.

## CLI Reference

```text
usage: scripts/tune.py [options]

  --trials N            Total Optuna trials (default 200)
  --n-jobs N            Parallel worker processes (default 8)
  --seeds N             Seed count per trial (default 3)
  --steps N             Episode step cap for matches (default 240)
  --time-budget S       Study wall-clock seconds (default 3600)
  --storage URL         Optuna storage URL (default sqlite:///tune.db)
  --study-name NAME     Stable study identifier
  --base-seed N         Seed set base offset (default 42)
  --timeout S           Per-match wall-clock cap (default 30)
  --promote             Write the champion back on success
```

## Worked Examples

Smoke study:

```bash
PYTHONPATH=build python scripts/tune.py \
  --trials 4 --n-jobs 2 --seeds 1 --steps 120 \
  --time-budget 300 --timeout 30 \
  --storage sqlite:////tmp/kaggriculture-smoke.db --study-name smoke
```

Production study:

```bash
PYTHONPATH=build python scripts/tune.py \
  --trials 500 --n-jobs 16 --seeds 5 --steps 720 \
  --time-budget 43200 --timeout 60 \
  --storage sqlite:///tune.db --study-name kaggriculture-vs-league --promote
```

Use `make tune` to run the defaults.
