#pragma once

#include <QJsonValue>
#include <QList>
#include <QString>

namespace tp {

enum class Mod { Shift = 0, Ctrl, Alt, Super };
constexpr int ModCount = 4;

// What a tap, swipe or long-press does.
//
// JSON forms (see layouts/README.md):
//   "x"                        -> commit text "x" (strings are ALWAYS literal text)
//   {"key": "ctrl+shift+c"}    -> real key event(s) via uinput
//   {"mod": "ctrl"}            -> modifier toggle (off -> one-shot -> locked -> off)
//   {"layer": "numeric"}       -> switch layer
//   {"hide": true}             -> hide the keyboard
//   {"command": "toggle-reserve"} -> keep screen space free above the keyboard
//   {"command": "reset-position"} -> move the blocks back to their default place
// Any object form may also carry "label" to override the displayed text.
struct Action {
    enum class Type { None, Text, Key, Mod, Layer, Hide, Command };

    Type type = Type::None;
    QString text;      // Text
    QString keySpec;   // Key: the original spec, e.g. "ctrl+backspace"
    QList<int> keys;   // Key: evdev codes, pressed in order, released in reverse
    Mod mod = Mod::Shift; // Mod
    QString layer;     // Layer
    QString command;   // Command: "toggle-reserve" or "reset-position"
    QString label;     // display override

    bool isValid() const { return type != Type::None; }
};

// Returns an Action with Type::None for JSON null. Sets *error on bad input.
Action parseAction(const QJsonValue &v, QString *error);

// Short label shown on the key face.
QString displayLabel(const Action &a, bool shifted);

// evdev keycode for a key name ("esc", "left", "f5", "a", ...), or -1.
int keyCodeByName(const QString &name);

// US-layout mapping used for Ctrl/Alt chords and for the no-input-method fallback.
bool asciiToKey(QChar c, int *code, bool *needShift);

QString modName(Mod m);
bool modFromName(const QString &name, Mod *out);

} // namespace tp
