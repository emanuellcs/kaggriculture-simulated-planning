"""Kaggle entrypoint for the Kaggriculture Simulated Planning engine.

The module owns the Python side of the runtime contract: import or JIT-compile
the native pybind11 extension, cache one C++ ``Engine`` per player, load the
tuned hyperparameters from ``config/hyperparameters.json``, and return the
``{"farmer": [...], "hands": [...], "market": [...]}`` action dictionary.
"""

from __future__ import annotations

import importlib
import json
import os
import subprocess
import sys
import sysconfig
import tempfile
from pathlib import Path

try:
    import kaggriculture_engine
except Exception:  # pragma: no cover
    kaggriculture_engine = None

_ENGINES = {}
_JIT_ATTEMPTED = False
try:
    _ROOT = Path(__file__).resolve().parent
except NameError:
    _ROOT = Path(os.getcwd()).resolve()


def _load_champion():
    try:
        data = json.loads((_ROOT / "config" / "hyperparameters.json").read_text())
    except Exception:
        return {}
    return dict(data.get("champion", {}))


_CHAMPION = _load_champion()


def _jit_log(message):
    print(f"[kaggriculture jit] {message}", file=sys.stderr, flush=True)


def _pybind11_include_dir():
    candidates = [_ROOT / "vendor" / "pybind11" / "include"]
    try:
        import pybind11

        candidates.append(Path(pybind11.get_include()))
    except Exception:
        pass
    for path in candidates:
        if (path / "pybind11" / "pybind11.h").exists():
            return path
    return None


def _python_include_dirs():
    paths = sysconfig.get_paths()
    include_dirs = []
    for key in ("include", "platinclude"):
        value = paths.get(key)
        if value and Path(value).exists() and value not in include_dirs:
            include_dirs.append(value)
    return include_dirs


def _compile_native_engine():
    sources = sorted((_ROOT / "src").glob("*.cpp"))
    if not sources:
        _jit_log("no C++ sources found under src/")
        return False
    pybind_include = _pybind11_include_dir()
    if pybind_include is None:
        _jit_log("pybind11 headers not found")
        return False
    python_includes = _python_include_dirs()
    if not python_includes:
        _jit_log("Python development headers not found")
        return False
    ext_suffix = sysconfig.get_config_var("EXT_SUFFIX") or ".so"
    output = _ROOT / f"kaggriculture_engine{ext_suffix}"
    command = [
        os.environ.get("CXX", "g++"),
        "-std=c++20", "-O3", "-DNDEBUG", "-fPIC", "-shared",
        "-ffast-math", "-march=native",
        "-Isrc/include", "-Isrc",
        f"-I{pybind_include}",
    ]
    command.extend(f"-I{path}" for path in python_includes)
    command.extend(str(path.relative_to(_ROOT)) for path in sources)
    command.extend(["-o", str(output)])
    _jit_log("compiling native engine")
    try:
        result = subprocess.run(command, cwd=_ROOT, capture_output=True, text=True, timeout=180)
    except Exception:
        _jit_log("compiler invocation failed")
        return False
    if result.returncode != 0:
        _jit_log(result.stderr)
        return False
    _jit_log(f"native engine built at {output.name}")
    return True


def _ensure_native_engine():
    global kaggriculture_engine, _JIT_ATTEMPTED
    if kaggriculture_engine is not None:
        return True
    if _JIT_ATTEMPTED:
        return False
    _JIT_ATTEMPTED = True
    for p in [_ROOT, _ROOT / "build", _ROOT / "lib"]:
        sos = list(p.glob("kaggriculture_engine*.so"))
        if sos:
            sys.path.insert(0, str(p))
            try:
                import kaggriculture_engine
                return True
            except ImportError:
                sys.path.pop(0)
    if not _compile_native_engine():
        return False
    try:
        importlib.invalidate_caches()
        if str(_ROOT) not in sys.path:
            sys.path.insert(0, str(_ROOT))
        kaggriculture_engine = importlib.import_module("kaggriculture_engine")
        return True
    except Exception:
        _jit_log("native engine import failed after JIT compile")
        return False


def _get(obj, name, default=None):
    if isinstance(obj, dict):
        return obj.get(name, default)
    return getattr(obj, name, default)


def set_hyperparameters(**kwargs):
    """Apply tuning hyperparameters to every cached engine."""
    global _CHAMPION
    _CHAMPION.update(kwargs)
    for engine in _ENGINES.values():
        try:
            engine.set_hyperparameters(_CHAMPION)
        except Exception:
            pass
    _ENGINES.clear()


def get_hyperparameters():
    return dict(_CHAMPION)


def _fallback_agent(obs, config):
    player = int(_get(obs, "player", 0))
    return {"farmer": ["PASS"], "hands": [], "market": []}


def agent(obs, config=None):
    """Kaggle-compatible agent entrypoint returning the action dictionary."""
    if not _ensure_native_engine():
        return _fallback_agent(obs, config)
    if kaggriculture_engine is None:
        return _fallback_agent(obs, config)

    player = int(_get(obs, "player", 0))
    engine = _ENGINES.get(player)
    if engine is None:
        engine = kaggriculture_engine.Engine()
        if _CHAMPION:
            engine.set_hyperparameters(_CHAMPION)
        _ENGINES[player] = engine

    engine.update_observation(obs, config)
    return engine.choose_actions(900)


if __name__ == "__main__":
    ok = _ensure_native_engine()
    print(f"kaggriculture_engine native available: {ok}")
