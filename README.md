# thumbpad

A swipe-gesture on-screen keyboard for Linux tablets (KDE Plasma, Wayland). It's built
for Czech, English and code, and it sends real Ctrl/Alt/Esc/Tab key events, so it also
works in terminals and Emacs.

> **Status:** early development. Nothing is usable yet.

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

## License

Public domain ([Unlicense](LICENSE)). Mentioning this project and the ones above is
appreciated, but not required. See [CREDITS.md](CREDITS.md).
