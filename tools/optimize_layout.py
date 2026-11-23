#!/usr/bin/env python3
"""Generates layouts/default.json from data/freq.json.

Model
-----
Each letter key has a tap and 8 swipes. Letters are placed in "groups": a base
letter plus its diacritic forms, which always share a key (the base letter
can be the tap or a swipe; the diacritics take swipe slots). The optimizer
decides which 18 letters are taps, which key every other letter goes on, and
(landscape) which thumb gets each letter.

Cost = sum(freq(char) * effort(slot)) + LAMBDA * same-thumb bigram weight

* effort(key): from the user's reach feedback, where the outer column (at the
  screen edge) and the top row are the hardest.
* effort(swipe) = effort(key) * SWIPE * direction factor. Swipes pointing at
  the screen edge, or at the bottom edge on the bottom row, are penalised
  (lesson from the Android layout).
* Space sits on both blocks, so bigrams with space don't count.

Then symbols fill the cheapest remaining slots by priority, with bracket pairs
on opposite slots of one key. Digits are long-press, a 1-9 phone pad on the
left block.

Usage: tools/optimize_layout.py [--restarts N] [--lambda L] [--seed S]
"""
import argparse
import json
import math
import multiprocessing as mp
import random
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DIRS = ["t", "tr", "r", "br", "b", "bl", "l", "tl"]
MIRROR = {"t": "t", "tr": "tl", "r": "l", "br": "bl", "b": "b", "bl": "br", "l": "r", "tl": "tr"}

# Key effort by [row][column counted from the screen edge]: 0 = outer column.
EFFORT = [
    [2.0, 1.6, 1.5],   # top row: hard (user feedback)
    [1.5, 1.0, 1.05],
    [1.5, 1.05, 1.15],
]
SWIPE = 2.0                       # a swipe costs this much more than a tap on the same key
DIAG = 1.15            # diagonal swipes are slightly harder

DIACRITICS = "áčďéěíňóřšťúůýž"
PUNCT = [".", "?", ","]  # optimised together with the letters
# Filled afterwards, in priority order. Tuples are pairs placed on opposite
# slots of the same key.
SYMBOLS = ["!", "-", ":", ";", '"', "'", ("(", ")"), "/", "@", "#", "_", "=", "+",
           "*", "&", "%", "$", ("<", ">"), ("[", "]"), ("{", "}"), "|", "\\", "~",
           "^", "`", ("„", "“"), "–", "€"]
OPPOSITE = [("l", "r"), ("tl", "tr"), ("bl", "br"), ("t", "b")]


def base_of(c):
    return unicodedata.normalize("NFD", c)[0]


# ---------------------------------------------------------------- key model

def slot_costs(row, col_out, mirrored):
    """Cost of tap and of each swipe (screen directions) for one key."""
    e = EFFORT[row][col_out]
    costs = {}
    for d in DIRS:  # directions as seen on a LEFT-edge block
        f = DIAG if len(d) == 2 else 1.0
        if d in ("l", "tl", "bl"):               # towards the screen edge
            f *= 2.0 if col_out == 0 else 1.4
        if row == 2 and d in ("b", "bl", "br"):  # towards the bottom edge
            f *= 1.4
        if row == 0 and d in ("t", "tl", "tr"):  # long stretch upwards
            f *= 1.2
        costs[MIRROR[d] if mirrored else d] = e * SWIPE * f
    return e, costs


def make_keys(blocks):
    """blocks: list of (side, mirrored). Returns key dicts, 9 per block."""
    keys = []
    for side, mirrored in blocks:
        for row in range(3):
            for col_out in range(3):
                tap, sw = slot_costs(row, col_out, mirrored)
                keys.append({"side": side, "row": row, "col_out": col_out,
                             "mirrored": mirrored, "tap": tap, "swipe": sw,
                             "order": sorted(sw.values())})
    return keys


# --------------------------------------------------------------- optimiser

class Problem:
    def __init__(self, freq, keys, lam):
        uni, bi, total = freq["unigrams"], freq["bigrams"], freq["chars"]
        self.keys = keys
        self.lam = lam
        self.groups = [chr(c) for c in range(ord("a"), ord("z") + 1)] + PUNCT
        self.members = {g: [g] for g in self.groups}
        for d in DIACRITICS:
            self.members[base_of(d)].append(d)
        self.f = {c: uni.get(c, 0) / total for g in self.groups for c in self.members[g]}
        # Group-level bigram weights (both orders, ignoring space/newline).
        gi = {c: i for i, g in enumerate(self.groups) for c in self.members[g]}
        n = len(self.groups)
        btotal = sum(bi.values())
        self.W = [[0.0] * n for _ in range(n)]
        for k, v in bi.items():
            a, b = k[0], k[1]
            if a in gi and b in gi and gi[a] != gi[b]:
                self.W[gi[a]][gi[b]] += v / btotal
                self.W[gi[b]][gi[a]] += v / btotal
        self.wtotal = sum(map(sum, self.W)) / 2
        self.n_letters = 26

    def side_penalty(self, side):
        n = len(self.groups)
        s = 0.0
        for i in range(n):
            si, Wi = side[i], self.W[i]
            for j in range(i + 1, n):
                if side[j] == si:
                    s += Wi[j]
        return s


def evaluate_key(P, k, tap, swipes):
    key = P.keys[k]
    items = [] if tap is None else list(P.members[tap][1:])
    for g in swipes:
        items.extend(P.members[g])
    if len(items) > 8:
        return 1e9
    items.sort(key=lambda c: -P.f[c])
    cost = 0.0 if tap is None else P.f[tap] * key["tap"]
    for c, sc in zip(items, key["order"]):
        cost += P.f[c] * sc
    return cost


def anneal(args):
    P, seed, iters = args
    rnd = random.Random(seed)
    K = len(P.keys)
    G = len(P.groups)
    gidx = {g: i for i, g in enumerate(P.groups)}
    letters = P.groups[:26]
    # Random start: K random letters are taps, the rest spread over keys.
    order = letters[:]
    rnd.shuffle(order)
    tap = order[:K]
    swipes = [[] for _ in range(K)]
    for g in order[K:] + PUNCT:
        swipes[rnd.randrange(K)].append(g)

    def side_vec():
        side = [0] * G
        for k in range(K):
            side[gidx[tap[k]]] = P.keys[k]["side"]
            for g in swipes[k]:
                side[gidx[g]] = P.keys[k]["side"]
        return side

    kc = [evaluate_key(P, k, tap[k], swipes[k]) for k in range(K)]
    side = side_vec()
    pen = P.side_penalty(side)
    cur = sum(kc) + P.lam * pen
    best = (cur, tap[:], [s[:] for s in swipes])

    T0, T1 = 2e-3, 1e-6
    for it in range(iters):
        T = T0 * (T1 / T0) ** (it / iters)
        m = rnd.random()
        if m < 0.3:  # swap two taps
            a, b = rnd.sample(range(K), 2)
            tap[a], tap[b] = tap[b], tap[a]
            touched = (a, b)
            def undo(a=a, b=b):
                tap[a], tap[b] = tap[b], tap[a]
        elif m < 0.55:  # move a swipe group to another key
            src = rnd.randrange(K)
            if not swipes[src]:
                continue
            i = rnd.randrange(len(swipes[src]))
            dst = rnd.randrange(K)
            if dst == src:
                continue
            g = swipes[src].pop(i)
            swipes[dst].append(g)
            touched = (src, dst)
            def undo(src=src, dst=dst, i=i, g=g):
                swipes[dst].remove(g)
                swipes[src].insert(i, g)
        elif m < 0.8:  # swap a tap letter with a swipe letter
            src = rnd.randrange(K)
            cands = [i for i, g in enumerate(swipes[src]) if g not in PUNCT]
            if not cands:
                continue
            i = rnd.choice(cands)
            k = rnd.randrange(K)
            g = swipes[src][i]
            old = tap[k]
            tap[k] = g
            swipes[src][i] = old
            touched = (src, k)
            def undo(src=src, i=i, k=k, g=g, old=old):
                tap[k] = old
                swipes[src][i] = g
        else:  # swap two swipe groups between keys
            a, b = rnd.sample(range(K), 2)
            if not swipes[a] or not swipes[b]:
                continue
            i, j = rnd.randrange(len(swipes[a])), rnd.randrange(len(swipes[b]))
            swipes[a][i], swipes[b][j] = swipes[b][j], swipes[a][i]
            touched = (a, b)
            def undo(a=a, b=b, i=i, j=j):
                swipes[a][i], swipes[b][j] = swipes[b][j], swipes[a][i]

        old_kc = [kc[t] for t in touched]
        for t in set(touched):
            kc[t] = evaluate_key(P, t, tap[t], swipes[t])
        new_side = side_vec() if P.lam else side
        new_pen = P.side_penalty(new_side) if P.lam else 0.0
        new = sum(kc) + P.lam * new_pen
        if new <= cur or rnd.random() < math.exp((cur - new) / T):
            cur, side, pen = new, new_side, new_pen
            if cur < best[0]:
                best = (cur, tap[:], [s[:] for s in swipes])
        else:
            undo()
            for t, v in zip(touched, old_kc):
                kc[t] = v
    return best


def optimise(P, restarts, iters, seed):
    jobs = [(P, seed + r, iters) for r in range(restarts)]
    with mp.Pool() as pool:
        results = pool.map(anneal, jobs)
    results.sort(key=lambda r: r[0])
    return results[0], [r[0] for r in results]


# ------------------------------------------------------------ slot filling

def fill_slots(P, keys, tap, swipes):
    """Returns per-key dict slot->char, applying the greedy slot order."""
    out = []
    for k, key in enumerate(keys):
        items = list(P.members[tap[k]][1:])
        for g in swipes[k]:
            items.extend(P.members[g])
        items.sort(key=lambda c: -P.f[c])
        slots = sorted(key["swipe"], key=lambda d: key["swipe"][d])
        m = {"tap": tap[k]}
        for c, d in zip(items, slots):
            m[d] = c
        out.append(m)
    return out


def place_symbols(keys, placed, symbols):
    dropped = []
    for sym in symbols:
        best = None
        for k, key in enumerate(keys):
            free = [d for d in DIRS if d not in placed[k]]
            if isinstance(sym, tuple):
                for a, b in OPPOSITE:
                    # Opening bracket on the left/top slot, closing on the other.
                    if a in free and b in free:
                        c = key["swipe"][a] + key["swipe"][b]
                        if best is None or c < best[0]:
                            best = (c, k, (a, b))
            else:
                for d in free:
                    c = key["swipe"][d]
                    if best is None or c < best[0]:
                        best = (c, k, (d,))
        if best is None:
            dropped.append(sym)
            continue
        _, k, ds = best
        for s, d in zip(sym if isinstance(sym, tuple) else (sym,), ds):
            placed[k][d] = s
    return dropped


def stats(P, keys, tap, swipes):
    side = {}
    for k in range(len(keys)):
        for g in [tap[k]] + swipes[k]:
            side[g] = keys[k]["side"]
    gi = {g: i for i, g in enumerate(P.groups)}
    same = sum(P.W[gi[a]][gi[b]] for a in P.groups for b in P.groups
               if gi[a] < gi[b] and side[a] == side[b])
    tapshare = sum(P.f[t] for t in tap) / sum(P.f[c] for g in P.groups for c in P.members[g])
    left = sum(P.f[c] for g in P.groups for c in P.members[g] if side[g] == 0)
    allf = sum(P.f[c] for g in P.groups for c in P.members[g])
    return {"alternation": 1 - same / P.wtotal if P.wtotal else 1, "tapShare": tapshare,
            "leftShare": left / allf}


# ------------------------------------------------------------- JSON output

def key_json(slotmap, long=None):
    k = {"tap": slotmap["tap"]}
    if long:
        k["long"] = long
    sw = {d: slotmap[d] for d in DIRS if d in slotmap}
    if sw:
        k["swipe"] = sw
    return k


ESC = {"tap": {"key": "esc"}, "long": {"hide": True},
       "swipe": {"r": {"key": "tab"}, "b": {"key": "shift+tab"}}}
MODS = {"tap": {"mod": "shift"}, "long": {"command": "toggle-reserve"},
        "swipe": {"t": {"mod": "ctrl"}, "r": {"mod": "alt"}, "b": {"mod": "super"}}}
SPACE_SLIDE = {"tap": " ", "slide": "cursor"}
SPACE_ENTER = {"tap": " ", "swipe": {"t": {"key": "enter"}, "l": {"key": "tab"}}}
BKSP = {"tap": {"key": "backspace"}, "repeat": True,
        "swipe": {"l": {"key": "ctrl+backspace"}, "r": {"key": "delete"}}}
ENTER = {"tap": {"key": "enter"}}


def layer_key(name):
    return {"tap": {"layer": name}, "long": {"command": "reset-position"}, "slide": "move"}


def landscape_rows(slots, layer_main=True):
    """slots: 18 key dicts (9 left then 9 right) in (row, col_out) order."""
    left = slots[:9]
    right = slots[9:]
    rows_l, rows_r = [], []
    specials_l = [ESC, MODS, SPACE_SLIDE]
    specials_r = [layer_key("numeric" if layer_main else "main"), BKSP, SPACE_ENTER]
    for r in range(3):
        lk = [left[r * 3 + c] for c in range(3)]          # outer, middle, inner
        rk = [right[r * 3 + c] for c in range(3)]
        rows_l.append(lk + [specials_l[r]])               # edge on the left
        rows_r.append([specials_r[r]] + rk[::-1])         # edge on the right
    return [{"edge": "left", "rows": rows_l}, {"edge": "right", "rows": rows_r}]


def portrait_block(slots, layer_main=True):
    sp = [layer_key("numeric" if layer_main else "main"), BKSP, MODS]
    rows = [[sp[r]] + [slots[r * 3 + c] for c in range(3)][::-1] for r in range(3)]
    rows.append([ESC, {"tap": " ", "slide": "cursor", "w": 2},
                 {"tap": {"key": "enter"}, "swipe": {"t": {"key": "tab"}}}])
    return [{"edge": "right", "rows": rows}]


def numeric_layers():
    def d(n, sw):
        return {"tap": n, "swipe": sw}
    pad = [d("1", {"t": "!"}), d("2", {"t": "@"}), d("3", {"t": "#"}),
           d("4", {"t": "$"}), d("5", {"t": "%"}), d("6", {"t": "^"}),
           d("7", {"t": "&"}), d("8", {"t": "*"}), d("9", {"t": "("})]
    nav = [
        {"tap": "0", "swipe": {"t": ")", "r": ".", "b": ","}},
        {"tap": {"key": "up"}, "repeat": True, "swipe": {"t": {"key": "pageup"}}},
        {"tap": {"key": "f5"}, "swipe": {"t": {"key": "f1"}, "tr": {"key": "f2"}, "r": {"key": "f3"},
                                         "br": {"key": "f4"}, "b": {"key": "f6"}, "bl": {"key": "f7"},
                                         "l": {"key": "f8"}, "tl": {"key": "f12"}}},
        {"tap": {"key": "left"}, "repeat": True, "swipe": {"t": {"key": "home"}}},
        {"tap": {"key": "down"}, "repeat": True, "swipe": {"b": {"key": "pagedown"}}},
        {"tap": {"key": "right"}, "repeat": True, "swipe": {"t": {"key": "end"}}},
        {"tap": "+", "swipe": {"t": "-", "r": "=", "b": "€"}},
        {"tap": "[", "swipe": {"t": "]", "r": "{", "b": "}"}},
        {"tap": "<", "swipe": {"t": ">", "r": "|", "b": "\\"}},
    ]
    # nav is listed in (row, col_out) order for the right block.
    land = landscape_rows(pad + nav, layer_main=False)
    port_pad = [d("1", {"b": "!", "l": "+"}), d("2", {"b": "@", "l": "-"}), d("3", {"b": "#", "l": "*"}),
                d("4", {"b": "$", "l": "/"}), d("5", {"b": "%", "l": "="}), d("6", {"b": "^", "l": "."}),
                d("7", {"b": "&", "l": "("}), d("8", {"b": "*", "l": ")"}), d("9", {"b": "(", "l": ","})]
    # portrait_block lists keys in (row, col_out) order; reverse rows so 1-2-3 reads left to right.
    port_slots = []
    for r in range(3):
        port_slots += port_pad[r * 3:r * 3 + 3][::-1]
    port = portrait_block(port_slots, layer_main=False)
    port[0]["rows"][3][1] = {"tap": "0", "w": 2, "swipe": {"t": ".", "l": ",", "r": ")"}}
    return {"landscape": land, "portrait": port}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--restarts", type=int, default=64)
    ap.add_argument("--iters", type=int, default=40000)
    ap.add_argument("--lambda", dest="lam", type=float, default=1.0)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--out", default=str(ROOT / "layouts/default.json"))
    a = ap.parse_args()

    freq = json.load(open(ROOT / "data/freq.json", encoding="utf-8"))

    # Landscape: left block (side 0) and mirrored right block (side 1).
    keys_l = make_keys([(0, False), (1, True)])
    P = Problem(freq, keys_l, a.lam)
    (cost, tap, swipes), all_costs = optimise(P, a.restarts, a.iters, a.seed)
    st = stats(P, keys_l, tap, swipes)
    slots_l = fill_slots(P, keys_l, tap, swipes)
    dropped_l = place_symbols(keys_l, slots_l, SYMBOLS)

    # Portrait: one right-edge block, one thumb, so no alternation term.
    keys_p = make_keys([(0, True)])
    P2 = Problem(freq, keys_p, 0.0)
    (cost_p, tap_p, swipes_p), all_p = optimise(P2, a.restarts, a.iters, a.seed + 1000)
    slots_p = fill_slots(P2, keys_p, tap_p, swipes_p)
    dropped_p = place_symbols(keys_p, slots_p, SYMBOLS)

    # Digits: long-press phone pad on the left block (1-2-3 on the top row,
    # read left to right on screen = outer to inner), 0 on the right centre key.
    main_slots = []
    for k, sm in enumerate(slots_l):
        long = None
        if k < 9:
            long = str(k + 1)
        elif k == 9 + 4:
            long = "0"
        main_slots.append(key_json(sm, long))
    port_slots = [key_json(sm) for sm in slots_p]

    layout = {
        "name": "thumbpad cz",
        "comment": "Generated by tools/optimize_layout.py from data/freq.json. "
                   "Edit by hand freely; rerunning the optimizer overwrites it.",
        "settings": {
            "keyMm": 15, "gapMm": 1.2, "swipeThresholdMm": 3.5, "longPressMs": 450,
            "opacity": 0.92,
            "requiredChars": "abcdefghijklmnopqrstuvwxyz" + DIACRITICS + ".,?!-:;'\"/()@#_=+*",
        },
        "layers": {
            "main": {"landscape": landscape_rows(main_slots),
                     "portrait": portrait_block(port_slots)},
            "numeric": numeric_layers(),
        },
    }
    with open(a.out, "w", encoding="utf-8") as f:
        json.dump(layout, f, ensure_ascii=False, indent=1)

    spread = sorted(all_costs)
    print(f"landscape: best cost {cost:.5f} (restarts: best {spread[0]:.5f}, "
          f"median {spread[len(spread) // 2]:.5f}, worst {spread[-1]:.5f})")
    print(f"  thumb alternation {100 * st['alternation']:.1f}% of letter bigrams, "
          f"taps cover {100 * st['tapShare']:.1f}% of letters, "
          f"left thumb {100 * st['leftShare']:.1f}% of letter presses")
    print(f"  left taps:  {' '.join(tap[:9])}\n  right taps: {' '.join(tap[9:])}")
    if dropped_l:
        print(f"  symbols not placed: {dropped_l}")
    print(f"portrait: best cost {cost_p:.5f}, taps {' '.join(tap_p)}")
    if dropped_p:
        print(f"  symbols not placed (numeric layer only): {dropped_p}")
    print(f"wrote {a.out}")


if __name__ == "__main__":
    main()
