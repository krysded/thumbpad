#!/usr/bin/env bash
# Switch KWin's virtual keyboard between thumbpad and plasma-keyboard.
#
#   scripts/kwin-use.sh dev      # this build tree's binary (build/thumbpad)
#   scripts/kwin-use.sh thumbpad # the installed one
#   scripts/kwin-use.sh plasma   # back to plasma-keyboard
#   scripts/kwin-use.sh status
#
# KWin starts the configured program itself and hands it the input-method
# protocol; it restarts the program if it crashes.
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
apps="${XDG_DATA_HOME:-$HOME/.local/share}/applications"

case "${1:-status}" in
dev)
    bin="$repo/build/thumbpad"
    [[ -x "$bin" ]] || { echo "build first: cmake --build build" >&2; exit 1; }
    mkdir -p "$apps"
    desktop="$apps/io.github.krysded.thumbpad-dev.desktop"
    sed "s|^Exec=.*|Exec=$bin|; s|^Name=.*|Name=thumbpad (dev build)|" \
        "$repo/data/io.github.krysded.thumbpad.desktop.in" > "$desktop"
    target="$desktop"
    ;;
thumbpad)
    target=""
    for dir in "$apps" /usr/local/share/applications /usr/share/applications; do
        if [[ -f "$dir/io.github.krysded.thumbpad.desktop" ]]; then
            target="$dir/io.github.krysded.thumbpad.desktop"
            break
        fi
    done
    [[ -n "$target" ]] || { echo "not installed: cmake --install build" >&2; exit 1; }
    ;;
plasma)
    target="/usr/share/applications/org.kde.plasma.keyboard.desktop"
    ;;
status)
    echo "InputMethod=$(kreadconfig6 --file kwinrc --group Wayland --key InputMethod)"
    echo "VirtualKeyboardEnabled=$(kreadconfig6 --file kwinrc --group Wayland --key VirtualKeyboardEnabled)"
    exit 0
    ;;
*)
    echo "usage: $0 dev|thumbpad|plasma|status" >&2
    exit 1
    ;;
esac

# KWin grants restricted protocols (the lock screen overlay) based on the
# .desktop file, which it finds through the KDE service cache.
kbuildsycoca6 >/dev/null 2>&1 || true
kwriteconfig6 --notify --file kwinrc --group Wayland --key InputMethod "$target"
kwriteconfig6 --notify --file kwinrc --group Wayland --key VirtualKeyboardEnabled true
qdbus6 org.kde.KWin /KWin reconfigure
echo "KWin input method -> $target"
