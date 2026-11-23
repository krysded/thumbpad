# thumbpad

A swipe-gesture on-screen keyboard for Linux tablets (KDE Plasma, Wayland). It's built
for Czech, English and code, and it sends real Ctrl/Alt/Esc/Tab key events, so it also
works in terminals and Emacs.

> **Status:** early, but in daily use on a Surface Pro 6 (Arch Linux, Plasma 6.7).

## Inspiration

thumbpad wouldn't exist without two projects:

- **[Thumb-Key](https://github.com/dessalines/thumb-key)**: an open-source Android
  keyboard with large keys where you tap or swipe in 8 directions. thumbpad borrows its
  interaction model and its "type-split" approach. The author's Czech Thumb-Key layout is
  where this project started.
- **[MessagEase](https://www.exideas.com/ME/)**: the original 3×3 swipe keyboard. The
  portrait one-handed layout is MessagEase-style.

See [CREDITS.md](CREDITS.md) for more.

## Planned design

- **Landscape:** two blocks at the screen edges, one per thumb. Each has 3×3 letter
  keys plus a column of special keys on the side toward the screen center.
- **Portrait:** one MessagEase-style block on the right, for one-handed use.
- **Gestures:** tap, 8-way swipe and long-press on every key.
- **Layout from data:** letter placement is optimized from real Czech text to maximize
  alternation between thumbs. One layout definition generates every mode (main,
  shifted, Ctrl, Alt, numeric).
- **Output:** text, including all Czech diacritics, is committed through KWin's input
  method protocol. Modifier chords, arrows, Esc and Tab are sent as real key events
  through uinput.

## Install

Needs Qt 6 (base with Wayland client), layer-shell-qt, cmake and a C++20 compiler.

```sh
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build build && ctest --test-dir build
cmake --install build
scripts/kwin-use.sh thumbpad   # make it KWin's virtual keyboard
scripts/kwin-use.sh plasma     # switch back to plasma-keyboard
```

thumbpad sends real key events through `/dev/uinput`. If your user can't write
to it, install `udev/60-thumbpad-uinput.rules` (see the comments in that file).

When the keyboard is hidden, a small ⌨ tab on the right screen edge brings it back.

Swipe up on the right `123` key (in portrait: on Esc) to open the settings panel:
key size, opacity, swipe and long-press timing, key repeat, reserved space, and the
layout file. Changes apply live and are saved to `~/.config/thumbpad/settings.ini`.

## Layout

The layout is one JSON file, documented in [layouts/README.md](layouts/README.md).
To customise it, copy or symlink it to `~/.config/thumbpad/layout.json`. The
running keyboard reloads it whenever you save. `thumbpad --check` validates a
layout, and `thumbpad --preview out.png` draws it to scale.

The default layout is generated from the author's own Czech text:
`tools/extract_freq.py` counts characters and bigrams, and
`tools/optimize_layout.py` places letters by simulated annealing. It minimises
reach effort plus same-thumb bigrams.

## License

Public domain ([Unlicense](LICENSE)). Mentioning this project and the ones above is
appreciated, but not required. See [CREDITS.md](CREDITS.md).
