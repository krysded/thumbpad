#!/usr/bin/env python3
"""Imports a Thumb-Key Kotlin layout (MAIN and NUMERIC modes) into JSON data.

The author's Czech Thumb-Key layout is the starting point for thumbpad's
landscape layout, so this keeps the Android file as the source of truth.
Only key contents are read (taps, swipes, long-press, modifier toggles); no
Thumb-Key code is copied.

Output: data/thumbkey_cz.json with {"main": rows, "numeric": rows}. A row
is a list of cells. A cell is either {"special": NAME} for Thumb-Key's
built-in keys or {"tap":..., "long":..., "swipe": {dir: action}}. Actions
are thumbpad JSON actions.

Usage: tools/import_thumbkey.py PATH/TO/CZTypeSplit.kt [out.json]
"""
import json
import re
import sys

DIRS = {"top": "t", "topRight": "tr", "right": "r", "bottomRight": "br",
        "bottom": "b", "bottomLeft": "bl", "left": "l", "topLeft": "tl"}
STRING = re.compile(r'"((?:[^"\\]|\\.)*)"')


def kotlin_string(s):
    out, i = [], 0
    while i < len(s):
        c = s[i]
        if c == "\\":
            n = s[i + 1]
            if n == "u":
                out.append(chr(int(s[i + 2:i + 6], 16)))
                i += 6
                continue
            out.append({"n": "\n", "t": "\t"}.get(n, n))
            i += 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


def balanced(text, start):
    """text[start] == '(' -> index just past the matching ')'. Skips strings."""
    depth, i = 0, start
    while i < len(text):
        c = text[i]
        if c == '"':
            m = STRING.match(text, i)
            i = m.end()
            continue
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    raise ValueError("unbalanced parentheses")


def parse_keyc(body):
    """Body of a KeyC(...) call -> thumbpad action (or None to drop it)."""
    if "ToggleCtrlMode(true)" in body:
        return {"mod": "ctrl"}
    if "ToggleAltMode(true)" in body:
        return {"mod": "alt"}
    if "ToggleShiftMode(true)" in body:
        return {"mod": "shift"}
    if "ToggleShiftMode(false)" in body or "Toggle" in body:
        return None  # "shift off": one-shot/cycling modifiers make it redundant
    m = STRING.search(body)
    return kotlin_string(m.group(1)) if m else None


def parse_item(block):
    """Inside of KeyItemC(...) -> cell dict."""
    cell = {}
    m = re.search(r"longPress\s*=\s*CommitText\(", block)
    if m:
        s = STRING.search(block, m.end())
        cell["long"] = kotlin_string(s.group(1))
    swipes = {}
    for m in re.finditer(r"\b(center|topLeft|topRight|top|bottomLeft|bottomRight|bottom|left|right)\s*=\s*", block):
        k = block.find("KeyC(", m.end())
        if k < 0 or block[m.end():k].strip():
            continue
        end = balanced(block, k + 4)
        action = parse_keyc(block[k + 5:end - 1])
        if action is None:
            continue
        if m.group(1) == "center":
            cell["tap"] = action
        else:
            swipes[DIRS[m.group(1)]] = action
    if swipes:
        cell["swipe"] = swipes
    return cell


def parse_mode(src, name):
    m = re.search(r"val\s+%s\s*=" % re.escape(name), src)
    if not m:
        raise SystemExit(f"{name} not found")
    start = src.index("KeyboardC(", m.end())
    body = src[start:balanced(src, start + 9)]
    rows = []
    # Rows are the inner listOf(...) calls of the outer listOf(...).
    outer = body.index("listOf(")
    pos = outer + len("listOf(")
    end = balanced(body, outer + 6)
    while True:
        r = body.find("listOf(", pos, end)
        if r < 0:
            break
        rend = balanced(body, r + 6)
        row_text = body[r + 7:rend - 1]
        cells, i = [], 0
        for it in re.finditer(r"KeyItemC\(|\b([A-Z][A-Z_]+)\b", row_text):
            if it.start() < i:
                continue
            if it.group(0) == "KeyItemC(":
                iend = balanced(row_text, it.end() - 1)
                cells.append(parse_item(row_text[it.end():iend - 1]))
                i = iend
            elif it.group(1).endswith(("_ITEM", "_DIRECTIONS")):
                cells.append({"special": it.group(1)})
                i = it.end()
        rows.append(cells)
        pos = rend
    return rows


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    src = open(sys.argv[1], encoding="utf-8").read()
    out = sys.argv[2] if len(sys.argv) > 2 else "data/thumbkey_cz.json"
    data = {
        "source": "Thumb-Key layout KB_CZ_TYPESPLIT_FULL (author's own design)",
        "main": parse_mode(src, "KB_CZ_TYPESPLIT_FULL_MAIN"),
        "numeric": parse_mode(src, "KB_CZ_TYPESPLIT_FULL_NUMERIC"),
    }
    with open(out, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=1)
    for mode in ("main", "numeric"):
        print(f"{mode}:")
        for row in data[mode]:
            print("   " + " | ".join(
                c.get("special") or f"{c.get('tap')}"
                + (f" ({c['long']})" if "long" in c else "")
                + "".join(f" {d}:{v if isinstance(v, str) else v['mod']}"
                          for d, v in c.get("swipe", {}).items())
                for c in row))


if __name__ == "__main__":
    main()
