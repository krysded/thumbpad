# Layout format

A layout is one JSON file. thumbpad loads `~/.config/thumbpad/layout.json` if it
exists, otherwise `default.json`. You can also pass `--layout FILE`. The running
keyboard **reloads the file as soon as you save it**.

Check a layout and look at it before using it:

```sh
build/thumbpad --layout my.json --check            # errors, warnings, table of every key
build/thumbpad --layout my.json --preview out.png  # to-scale picture of every layer
```

## Structure

```jsonc
{
  "name": "my layout",
  "settings": { "keyMm": 15, ... },          // all optional, see below
  "layers": {
    "main":    { "landscape": [BLOCK, ...], "portrait": [BLOCK, ...] },
    "numeric": { ... }                       // any other layer names you like
  }
}
```

A **block** is one rectangle of keys: `{"edge": "left" | "right" | "center", "rows": [[KEY, ...], ...]}`.
Landscape normally has a left and a right block. Portrait has one block. If a layer
has no `portrait` blocks, it reuses its `landscape` ones.

## Keys

```jsonc
"x"                                   // shorthand for {"tap": "x"}
null                                  // empty cell, 1 key wide
{"gap": 0.5}                          // empty cell of a custom width
{
  "tap":   ACTION,
  "long":  ACTION,                    // long-press
  "swipe": {"t": ACTION, "tr": ..., "r": ..., "br": ..., "b": ..., "bl": ..., "l": ..., "tl": ...},
  "repeat": true,                     // holding repeats the tap action (backspace, arrows)
  "slide": "cursor",                  // dragging moves the cursor (spacebar); no swipes allowed
  "w": 2,                             // width in keys
  "label": "⌫"                        // custom face text
}
```

Long direction names (`top`, `topRight`, `bottomLeft`, …) also work. The swipe
recogniser splits the circle into eight 45° sectors.

## Actions

| JSON | Effect |
|---|---|
| `"ě"`, `"->"`, `"@"` | Commits the text. **A plain string is always literal text**, so there is no escaping beyond JSON's `\"` and `\\`. |
| `{"key": "esc"}`, `{"key": "ctrl+shift+c"}` | Real key event(s) through uinput. |
| `{"mod": "shift" \| "ctrl" \| "alt" \| "super"}` | Modifier: tap once for the next key only, twice to lock, a third time to turn it off. Locked Shift is Caps Lock. |
| `{"layer": "numeric"}` | Switches layer. |
| `{"hide": true}` | Hides the keyboard. |

Any object form can also carry `"label"`.

Key names: `esc tab backspace enter delete insert space left right up down home end
pageup pagedown menu print capslock f1`–`f12 a`–`z 0`–`9 minus equal comma dot slash
semicolon apostrophe grave leftbrace rightbrace backslash volumeup volumedown mute
brightnessup brightnessdown`. Modifiers in chords: `ctrl shift alt altgr super`.

### How modifiers combine with text

There are no separate Ctrl/Alt grids to maintain:

- **Shift** uppercases text actions (`ž` → `Ž`).
- **Ctrl / Alt / Super** plus a single ASCII character sends a real chord. Ctrl + `a`
  is Ctrl+A, and Ctrl + Shift + `c` is Ctrl+Shift+C (copy in Konsole). Chords use the
  US key positions.
- Ctrl / Alt / Super plus a non-ASCII character (`č`) just types the character.
- Modifiers also apply to `{"key": …}` actions and to the cursor slide, so Shift +
  slide selects text.

## Settings

| key | default | meaning |
|---|---|---|
| `keyMm` | 15 | key size in millimetres (real size, from the screen's physical size) |
| `gapMm` | 1.2 | gap between keys |
| `swipeThresholdMm` | 3.5 | shorter movements count as a tap |
| `slideStepMm` | 2.5 | cursor slide distance per arrow press (vertical: ×2) |
| `edgeMarginMm`, `bottomMarginMm` | 2 | distance from the screen edges |
| `longPressMs` | 450 | long-press delay |
| `repeatDelayMs`, `repeatIntervalMs` | 400, 50 | key repeat |
| `opacity` | 0.92 | background opacity |
| `requiredChars` | "" | `--check` warns when any of these can't be typed from the `main` layer |
