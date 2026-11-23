# thumbpad

A swipe-gesture on-screen keyboard for Linux tablets running KDE Plasma on Wayland.
It's built for Czech, English and code, and it sends real Ctrl/Alt/Esc/Tab and arrow
key events, so it also works in terminals and Emacs.

> **Status:** early, but in daily use on a Surface Pro 6 (Arch Linux, Plasma 6.7).
> The default layout is Czech + English. Other languages need their own layout
> file (see [Layouts](#layouts)).

## Inspiration

thumbpad wouldn't exist without two projects:

- **[Thumb-Key](https://github.com/dessalines/thumb-key)**: an open-source Android
  keyboard with large keys where you tap or swipe in 8 directions. thumbpad borrows its
  interaction model and its "type-split" approach. The author's Czech Thumb-Key layout is
  where this project started, and the landscape layout keeps its key positions.
- **[MessagEase](https://www.exideas.com/ME/)**: the original 3×3 swipe keyboard. The
  portrait one-handed layout is MessagEase-style.

See [CREDITS.md](CREDITS.md) for more.

## Features

- **Landscape:** a split keyboard, one block per thumb at the bottom screen edges.
- **Portrait:** one MessagEase-style block on the right, for one-handed use.
- **Gestures:** tap, 8-way swipe and long-press on every key; key repeat; cursor
  movement by sliding along the space bar.
- **Real modifiers:** Shift, Ctrl, Alt and Super work as one-shot or locked modifiers
  and combine with every letter, arrow and special key (Ctrl+A, Ctrl+Shift+C, Alt+F, …).
- **Any Unicode text:** text, including all Czech diacritics, is committed through
  KWin's input-method protocol, independent of your keyboard layout setting.
- **Movable:** drag the keyboard where you want it; optionally make windows shrink so
  the keyboard never covers what you type.
- **Settings panel** with live preview, and a **plain JSON layout format** that is
  reloaded as soon as you save it.

## Requirements

- **KDE Plasma 6 on Wayland.** thumbpad uses KWin's input-method protocol
  (`zwp_input_method_v1`) and the layer-shell protocol. Other desktops are not
  supported.
- **Write access to `/dev/uinput`**, for real key events (see below).
- To build: a C++20 compiler, CMake ≥ 3.21, Qt ≥ 6.5 (base with the Wayland client
  module, declarative), layer-shell-qt and the Wayland client library.

On **Arch Linux**:

```sh
sudo pacman -S --needed base-devel cmake qt6-base qt6-declarative layer-shell-qt wayland qt6-tools kconfig
```

`qt6-tools` and `kconfig` provide `qdbus6` and `kwriteconfig6`, which the helper
script uses; on Plasma they are usually installed already. Older Qt releases ship the
Wayland client module separately (`qt6-wayland`). On other distributions, install the
equivalent development packages.

### uinput access

Check whether your user may already create virtual input devices:

```sh
test -w /dev/uinput && echo "uinput: ok" || echo "uinput: no access"
```

Plasma's KDE Connect ships a udev rule that grants this to the logged-in user, so
it often works out of the box. If not, install the rule that comes with thumbpad:

```sh
sudo cp udev/60-thumbpad-uinput.rules /etc/udev/rules.d/
sudo udevadm control --reload && sudo udevadm trigger /dev/uinput
```

Without uinput access thumbpad still types text, but Ctrl/Alt chords, arrows, Esc,
Tab, Backspace and Enter won't work.

## Install

```sh
git clone https://github.com/krysded/thumbpad.git
cd thumbpad
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build build
ctest --test-dir build
cmake --install build
```

Then make it Plasma's virtual keyboard:

```sh
scripts/kwin-use.sh thumbpad
```

This points KWin's input method at thumbpad and turns the virtual keyboard on, and
KWin starts thumbpad right away. From now on it appears whenever a text field is
focused.

To go back to Plasma's own keyboard at any time:

```sh
scripts/kwin-use.sh plasma
```

`scripts/kwin-use.sh status` shows the current setting.

### Update

```sh
git pull
cmake --build build && cmake --install build
pkill -x thumbpad   # KWin restarts it with the new version
```

### Uninstall

```sh
scripts/kwin-use.sh plasma
xargs rm -v < build/install_manifest.txt
rm -r ~/.config/thumbpad   # your settings and saved position, if you want them gone
```

## Using it

The default landscape layout:

```
LEFT                                RIGHT
[n ] [t ] [j ] [Esc]                [123] [y ] [e ] [a ]
[l ] [r ] [z ] [ ⇧ ]                [ ⌫ ] [c ] [o ] [i ]
[d ] [m ] [v ] [123]                [ ✥ ] [p ] [s ] [u ]
[    space     ] [⇥]                [ ⏎ ] [    space     ]
```

The small labels around a key show what a swipe in that direction types, and the
tiny digit next to the centre is the long-press.

| Gesture | Does |
|---|---|
| Tap / swipe / long-press a letter key | The letter / the label in that direction / the digit |
| Tap **⇧** | Shift for the next key; tap again to lock (Caps Lock); a third time turns it off |
| Swipe **⇧** up / right / down | Ctrl / Alt / Super, with the same once-or-locked behaviour |
| Slide along the left **space** | Moves the cursor (with Shift active: selects) |
| **⌫**: hold / swipe left / swipe right | Repeat / delete word / Delete |
| Swipe **✥** | Arrow keys; diagonals give Home, End, Page Up, Page Down |
| Tap **123** | Numbers and symbols layer (**abc** goes back) |
| Drag the left **123** | Moves the keyboard (both blocks move together) |
| Swipe up on the right **123** | Opens the settings panel |
| Long-press **123** | Puts the keyboard back in its default place |
| Long-press **Esc** | Hides the keyboard. The **⌨** tab on the right screen edge brings it back |
| Long-press **⇧** | Reserve space: windows shrink so the keyboard never covers them |

In **portrait**, the single block has the same special keys. Its **123** key is the
drag handle, and swiping up on **Esc** opens the settings.

### Settings

Key size, gap and opacity; swipe distance, long-press delay, key repeat and
cursor-slide speed; reserve space; the ⌨ tab; and which layout file to use. Changes
apply immediately, while the keyboard stays visible for a live preview. They are
saved to `~/.config/thumbpad/settings.ini` and override the layout's defaults.

### Command line and D-Bus

```sh
thumbpad --toggle                                   # show/hide the running keyboard
qdbus6 io.github.krysded.thumbpad / OpenSettings
qdbus6 io.github.krysded.thumbpad / Press '{"key":"ctrl+alt+t"}'
qdbus6 io.github.krysded.thumbpad / State
```

`thumbpad --toggle` is handy for a panel button or a global shortcut.

## Layouts

A layout is one JSON file, documented in [layouts/README.md](layouts/README.md).
thumbpad uses the first one it finds:

1. the file chosen in the settings panel,
2. `~/.config/thumbpad/layout.json`,
3. the installed default, `~/.local/share/thumbpad/layouts/default.json`.

To customise the default:

```sh
mkdir -p ~/.config/thumbpad
cp ~/.local/share/thumbpad/layouts/default.json ~/.config/thumbpad/layout.json
```

The running keyboard reloads the file whenever you save it. Errors are reported in
the log and the previous layout stays active. Extra layouts can go in
`~/.config/thumbpad/layouts/*.json`; the settings panel lists them.

Before using a layout, check it:

```sh
thumbpad --layout my.json --check            # errors, warnings and a table of every key
thumbpad --layout my.json --preview out.png  # to-scale picture of every layer
```

### How the default layout was made

The scripts in `tools/` need only Python 3:

- `extract_freq.py` counts characters and bigrams in a text archive (the author's own
  tweets, which are not in this repository; only the aggregate counts in
  `data/freq.json` are).
- `import_thumbkey.py` reads the author's Thumb-Key layout into `data/thumbkey_cz.json`.
- `optimize_layout.py` builds `layouts/default.json`. Landscape keeps the Thumb-Key key
  positions and moves the six best swipe-only letters to taps in a new column. Portrait
  is optimised from scratch by simulated annealing. Both minimise reach effort plus
  same-thumb bigrams.

The optimiser is currently tailored to Czech: the diacritics are a constant in
`optimize_layout.py`, and the landscape layout starts from the Thumb-Key import. For
another language, run `extract_freq.py` on a plain-text file of your own writing
(entries separated by lines containing only `---`), adjust `DIACRITICS` and the
landscape step, or simply write a layout by hand in the JSON format.

## Troubleshooting

- **Logs:** `journalctl --user -t thumbpad -f`.
- **The keyboard doesn't appear:** run `scripts/kwin-use.sh status`; `InputMethod`
  should point at thumbpad's `.desktop` file and `VirtualKeyboardEnabled` should be
  `true`. Check that it runs with `pgrep -a thumbpad`. KWin only shows the keyboard
  for apps that support text input (Qt and GTK apps do). For others, use the ⌨ tab or
  `thumbpad --toggle`. Without an input-method session, text falls back to plain ASCII
  typed via uinput.
- **Letters arrive but Ctrl/arrows/Backspace don't:** no uinput access (see above).
  The log says `cannot open /dev/uinput`.
- **Wrong characters with Ctrl chords:** chords use US key positions. They are correct
  with a US or Czech QWERTY system layout, but not with QWERTZ or AZERTY.
- **Locked out after hiding it:** tap the ⌨ tab on the right screen edge. If you turned
  the tab off, focus a new text field, or run `scripts/kwin-use.sh plasma` with a mouse
  or hardware keyboard.

## License

Public domain ([Unlicense](LICENSE)). Mentioning this project and the ones above is
appreciated, but not required. See [CREDITS.md](CREDITS.md).
