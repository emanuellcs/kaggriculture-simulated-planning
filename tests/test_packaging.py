"""Packaging and hot-path allocation-guard tests."""

from __future__ import annotations

import os
import subprocess
import sys
import tarfile
import tempfile

import package_submission

from _fixtures import REPO_ROOT


def test_hot_sources_avoid_dynamic_allocation_primitives():
    """Guard the simulator and tactical hot files against allocation primitives."""

    hot_files = [
        "src/kaggriculture_sim.cpp",
        "src/kaggriculture_tactical.cpp",
        "src/kaggriculture_plan.cpp",
    ]
    forbidden = ("std::vector", "std::map", "std::set", "make_unique", "std::function")
    for rel in hot_files:
        with open(os.path.join(REPO_ROOT, rel), encoding="utf-8") as handle:
            source = handle.read()
        for token in forbidden:
            assert token not in source


def test_packaged_submission_jit_compiles_in_extracted_directory():
    """The Kaggle source bundle must JIT-compile in a clean directory."""

    package_path = package_submission.build_package()
    env = os.environ.copy()
    env.pop("PYTHONPATH", None)
    with tempfile.TemporaryDirectory() as tmp:
        with tarfile.open(package_path, "r:gz") as tar:
            try:
                tar.extractall(tmp, filter="data")
            except TypeError:
                tar.extractall(tmp)
        smoke = subprocess.run(
            [sys.executable, "main.py"],
            cwd=tmp,
            env=env,
            capture_output=True,
            text=True,
            timeout=180,
        )
        assert smoke.returncode == 0, smoke.stdout + smoke.stderr
        assert "kaggriculture_engine native available: True" in smoke.stdout
