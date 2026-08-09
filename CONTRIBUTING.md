# Contributing

This guide covers the development workflow for the Kaggriculture engine: environment setup, building, testing, code style, and how to add a hyperparameter or an opponent archetype.

## Development Workflow

1. Create a feature branch from `main`.
2. Make the change, including regression tests for any market- or rules-sensitive behavior.
3. Run `make test` and `make diffcheck`.
4. Run `make bench` to measure the change against the opponent league.
5. If the change targets production planning, run a tuning study and promote the champion.
6. Open a pull request with a concise description and the benchmark result.

## Environment Setup

```bash
python -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install -r requirements-dev.txt
```

## Building

```bash
make build
```

The build produces the `kaggriculture_engine` extension under `build/`. Import it with `PYTHONPATH=build`. The `main.py` JIT path compiles the same sources with the equivalent flags and is exercised by the packaging tests.

## Testing

```bash
make test
```

The pytest suite lives in `tests/`:

| Module | Covers |
| --- | --- |
| `test_bridge.py` | Observation parsing, action format, hyperparameters, price helpers |
| `test_market.py` | Price curves, floor, shape functions |
| `test_sim.py` | Gameplay smoke and full-season outcome |
| `test_opponents.py` | Archetype registration |
| `test_packaging.py` | Hot-path allocation guard, Kaggle JIT packaging |

## Rules Fidelity

```bash
make diffcheck
```

`scripts/diffcheck.py` plays short games with the native agent and verifies the games complete without invalid or error statuses. Run it before changing any market or turn-handling behavior.

## Benchmarking

```bash
make bench
```

The benchmark harness plays the champion against every opponent on held-out seeds with seat rotation and reports win rates with a Wilson confidence interval.

## Code Style and Comment Policy

The repository follows a concise-API, minimal-internals policy.

### C++

- No `@file`, `@author`, or `@date` boilerplate. At most one `//` purpose line at the top of a large translation unit.
- Headers carry a concise `/// @brief` and only essential `@param` and `@return` annotations.
- Internal and static functions get no doc block. Add a single `//` comment only where the "why" is non-obvious.
- Comments explain why, not what. Delete stale and dead-code comments.
- Prefer a named `constexpr` over a magic-number comment.

### Python

- Module docstrings are one to three sentences describing purpose.
- Concise docstrings appear only on public entrypoints such as `main.agent`, `set_hyperparameters`, and CLI functions.
- Internal helpers get no docstring. Inline `#` comments appear only for non-obvious reasoning.

### Configuration

`config/hyperparameters.json` is the single source of truth for tuned parameters and search-space bounds. Do not hardcode tuned values in `main.py` or in C++ defaults.

## Adding a New Hyperparameter

1. Add the field to `Hyperparameters` in `src/include/engine.hpp`.
2. Wire it through `set_hyperparameters` and `get_hyperparameters` in `src/kaggriculture_bindings.cpp`.
3. Add the parameter name, type, bounds, and default to `config/hyperparameters.json`.
4. If the planner or scheduler should consume it, read it in `kaggriculture_plan.cpp` or `kaggriculture_tactical.cpp`.
5. Add a round-trip assertion in `tests/test_bridge.py`.
6. Run `make test`.

## Adding a New Opponent Archetype

1. Create `opponents/<name>.py` with an `agent(obs, config=None)` function returning the action dictionary and an `act` fallback wrapper.
2. Make the policy deterministic for a given seed so benchmark results are reproducible.
3. Register the archetype in `config/league.json` and add it to the parameterized test in `tests/test_opponents.py`.
4. Run `make test` and `make bench`.

## CI/CD

The GitHub Actions workflow builds and tests the project on Ubuntu for Python 3.11 and 3.12. It runs the pytest suite, the differential rules-fidelity check, a benchmark smoke, a profiling perf gate, an ASan/UBSan sanitizer job, and a source-package job.

## Troubleshooting

- `ModuleNotFoundError: kaggriculture_engine`: build the extension or set `PYTHONPATH=build`.
- JIT packaging failures: ensure pybind11 is installed, since `scripts/package_submission.py` vendors its headers into the bundle.
- Failing differential checks: the agent produces an illegal action or the turn handling diverged from the interpreter. Check `tests/test_sim.py` for the locked gameplay expectations.
