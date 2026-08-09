"""Shared helpers for hand-crafted Kaggriculture opponent archetypes."""

from collections import deque

DIRS = {"NORTH": (0, -1), "SOUTH": (0, 1), "EAST": (1, 0), "WEST": (-1, 0)}
CROPS = ["WHEAT", "CARROT", "TOMATO", "STRAWBERRY", "MELON"]
SEED_COST = {"WHEAT": 10, "CARROT": 20, "TOMATO": 50, "STRAWBERRY": 100, "MELON": 80}


def _get(o, k, d=None):
    if isinstance(o, dict):
        return o.get(k, d)
    return getattr(o, k, d)


def path_first(obs, player, sx, sy, goal_x, goal_y):
    """Return the first cardinal step toward a goal cell (or None)."""
    me = _get(obs, "farms")[player]
    bs = len(me["tiles"])
    if (sx, sy) == (goal_x, goal_y):
        return None
    prev = {}
    q = deque([(sx, sy)])
    seen = {(sx, sy)}
    while q:
        x, y = q.popleft()
        if (x, y) == (goal_x, goal_y):
            while prev.get((x, y)) is not None:
                nx, ny = prev[(x, y)]
                if (nx, ny) == (sx, sy):
                    for d, (dx, dy) in DIRS.items():
                        if x == sx + dx and y == sy + dy:
                            return d
                x, y = nx, ny
            return None
        for d, (dx, dy) in DIRS.items():
            nx, ny = x + dx, y + dy
            if 0 <= nx < bs and 0 <= ny < bs and (nx, ny) not in seen:
                seen.add((nx, ny))
                prev[(nx, ny)] = (x, y)
                q.append((nx, ny))
    return None


def nearest_empty(obs, player, sx, sy):
    me = _get(obs, "farms")[player]
    bs = len(me["tiles"])
    best = None
    best_d = None
    for y in range(bs):
        for x in range(bs):
            t = me["tiles"][y][x]
            if t is None:
                d = abs(x - sx) + abs(y - sy)
                if best_d is None or d < best_d:
                    best_d = d
                    best = (x, y)
    return best


def nearest_tile(obs, player, sx, sy, pred):
    me = _get(obs, "farms")[player]
    bs = len(me["tiles"])
    best = None
    best_d = None
    for y in range(bs):
        for x in range(bs):
            t = me["tiles"][y][x]
            if isinstance(t, dict) and pred(t):
                d = abs(x - sx) + abs(y - sy)
                if best_d is None or d < best_d:
                    best_d = d
                    best = (x, y)
    return best
