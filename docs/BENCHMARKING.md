# Benchmarking

This document describes how the Kaggriculture engine is measured: the benchmark harness, the evaluation protocol, the confidence interval, and the Elo ladder.

## Overview

`scripts/bench.py` is the propose-measure-keep loop. It plays the current champion against every opponent on held-out seeds with seat rotation and reports win rates with a Wilson confidence interval.

## Running the Harness

```bash
make bench
```

or:

```bash
PYTHONPATH=build python scripts/bench.py --seeds 20 --base-seed 9000 --steps 720 --timeout 60
```

The champion is loaded from `config/hyperparameters.json`. Opponents are the built-in `starter`/`random`/`pass`, the hand-crafted archetypes in `config/league.json`, and any self-play checkpoints.

## CLI Reference

```text
usage: scripts/bench.py [options]

  --seeds N            Held-out seed count (default 20)
  --base-seed N        Held-out seed offset (default 9000)
  --steps N            Episode step cap for matches (default 720)
  --timeout S          Per-match wall-clock cap (default 30)
```

## Evaluation Protocol

- A held-out seed set is separate from the tuning seed set.
- Every opponent is played in both seats on every seed.
- Matches are wall-clock bounded so a hung worker cannot stall the run.

## Confidence Interval

Win rates are reported with a Wilson score interval at 95 percent confidence:

```math
\frac{
  \hat{p} + \frac{z^2}{2n}
  \pm z\sqrt{\frac{\hat{p}(1 - \hat{p}) + z^2/(4n)}{n}}
}{1 + z^2/n}
```

## Elo Ladder

With `--elo` the harness updates league ratings in `config/league.json`. A standard Elo update with `K = 32` is applied based on the observed win rate. The ladder doubles as a progress meter across versions.

## Report Output

Reports can be written to `docs/benchmarks/` with the date, seed set, step cap, and per-opponent results so measurements remain reproducible.
