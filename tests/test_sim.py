"""Gameplay smoke tests: the agent completes full seasons legally."""

from __future__ import annotations

import pytest


@pytest.mark.parametrize("opponent", ["starter", "random", "pass"])
@pytest.mark.parametrize("steps", [120])
def test_agent_completes_game(opponent, steps):
    """The native agent must finish a short game without erroring."""
    import main
    from kaggle_environments import make

    env = make("kaggriculture", configuration={"episodeSteps": steps}, debug=False)
    env.run([main.agent, opponent])
    last = env.steps[-1]
    assert all(s.status in {"DONE", "ACTIVE", "TIMEOUT"} for s in last)
    assert not any(s.status == "ERROR" for s in last)


def test_agent_beats_pass_over_full_season():
    """The champion should out-earn a passive opponent on a full season."""
    import main
    from kaggle_environments import make

    env = make("kaggriculture", configuration={"episodeSteps": 720}, debug=False)
    env.run([main.agent, "pass"])
    last = env.steps[-1]
    assert float(last[0].reward) > float(last[1].reward)
    assert float(last[0].reward) > 3000.0
