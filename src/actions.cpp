#include "actions.h"

#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <linux/input-event-codes.h>

namespace tp {

namespace {

const QHash<QString, int> &keyTable()
{
    static const QHash<QString, int> t = [] {
        QHash<QString, int> h{
            {"esc", KEY_ESC}, {"escape", KEY_ESC}, {"tab", KEY_TAB},
            {"backspace", KEY_BACKSPACE}, {"enter", KEY_ENTER}, {"return", KEY_ENTER},
            {"delete", KEY_DELETE}, {"del", KEY_DELETE}, {"insert", KEY_INSERT},
            {"space", KEY_SPACE}, {"left", KEY_LEFT}, {"right", KEY_RIGHT},
            {"up", KEY_UP}, {"down", KEY_DOWN}, {"home", KEY_HOME}, {"end", KEY_END},
            {"pageup", KEY_PAGEUP}, {"pgup", KEY_PAGEUP},
            {"pagedown", KEY_PAGEDOWN}, {"pgdn", KEY_PAGEDOWN},
            {"menu", KEY_COMPOSE}, {"print", KEY_SYSRQ}, {"capslock", KEY_CAPSLOCK},
            {"minus", KEY_MINUS}, {"equal", KEY_EQUAL}, {"comma", KEY_COMMA},
            {"dot", KEY_DOT}, {"slash", KEY_SLASH}, {"semicolon", KEY_SEMICOLON},
            {"apostrophe", KEY_APOSTROPHE}, {"grave", KEY_GRAVE},
            {"leftbrace", KEY_LEFTBRACE}, {"rightbrace", KEY_RIGHTBRACE},
            {"backslash", KEY_BACKSLASH},
            {"volumeup", KEY_VOLUMEUP}, {"volumedown", KEY_VOLUMEDOWN}, {"mute", KEY_MUTE},
            {"brightnessup", KEY_BRIGHTNESSUP}, {"brightnessdown", KEY_BRIGHTNESSDOWN},
            {"ctrl", KEY_LEFTCTRL}, {"control", KEY_LEFTCTRL}, {"shift", KEY_LEFTSHIFT},
            {"alt", KEY_LEFTALT}, {"altgr", KEY_RIGHTALT},
            {"super", KEY_LEFTMETA}, {"meta", KEY_LEFTMETA}, {"win", KEY_LEFTMETA},
        };
        const int letters[] = {KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I,
                               KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R,
                               KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z};
        for (int i = 0; i < 26; ++i)
            h.insert(QString(QChar('a' + i)), letters[i]);
        const int digits[] = {KEY_0, KEY_1, KEY_2, KEY_3, KEY_4,
                              KEY_5, KEY_6, KEY_7, KEY_8, KEY_9};
        for (int i = 0; i < 10; ++i)
            h.insert(QString::number(i), digits[i]);
        const int fkeys[] = {KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
                             KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12};
        for (int i = 0; i < 12; ++i)
            h.insert("f" + QString::number(i + 1), fkeys[i]);
        return h;
    }();
    return t;
}

const QHash<QString, QString> &prettyKeyNames()
{
    static const QHash<QString, QString> t{
        {"esc", "Esc"}, {"escape", "Esc"}, {"tab", "⇥"}, {"backspace", "⌫"},
        {"enter", "⏎"}, {"return", "⏎"}, {"delete", "Del"}, {"del", "Del"},
        {"insert", "Ins"}, {"space", "␣"}, {"left", "←"}, {"right", "→"},
        {"up", "↑"}, {"down", "↓"}, {"home", "Home"}, {"end", "End"},
        {"pageup", "PgUp"}, {"pgup", "PgUp"}, {"pagedown", "PgDn"}, {"pgdn", "PgDn"},
        {"menu", "Menu"}, {"capslock", "Caps"},
    };
    return t;
}

// Emacs-style modifier prefixes keep chord labels short: C-⌫, M-x, S-⇥.
const QHash<QString, QString> &modPrefixes()
{
    static const QHash<QString, QString> t{
        {"ctrl", "C-"}, {"control", "C-"}, {"alt", "M-"}, {"shift", "S-"},
        {"super", "s-"}, {"meta", "s-"}, {"win", "s-"}, {"altgr", "AltGr-"},
    };
    return t;
}

const QSet<QString> &actionObjectFields()
{
    static const QSet<QString> s{"key", "mod", "layer", "hide", "text", "label", "command"};
    return s;
}

} // namespace

int keyCodeByName(const QString &name)
{
    return keyTable().value(name.toLower(), -1);
}

QString modName(Mod m)
{
    switch (m) {
    case Mod::Shift: return QStringLiteral("shift");
    case Mod::Ctrl: return QStringLiteral("ctrl");
    case Mod::Alt: return QStringLiteral("alt");
    case Mod::Super: return QStringLiteral("super");
    }
    return {};
}

bool modFromName(const QString &name, Mod *out)
{
    const QString n = name.toLower();
    if (n == "shift") *out = Mod::Shift;
    else if (n == "ctrl" || n == "control") *out = Mod::Ctrl;
    else if (n == "alt") *out = Mod::Alt;
    else if (n == "super" || n == "meta" || n == "win") *out = Mod::Super;
    else return false;
    return true;
}

Action parseAction(const QJsonValue &v, QString *error)
{
    Action a;
    if (v.isNull() || v.isUndefined())
        return a;

    if (v.isString()) {
        a.text = v.toString();
        if (a.text.isEmpty()) {
            *error = QStringLiteral("empty text action");
            return {};
        }
        a.type = Action::Type::Text;
        return a;
    }

    if (!v.isObject()) {
        *error = QStringLiteral("action must be a string, an object or null");
        return {};
    }

    const QJsonObject o = v.toObject();
    int kinds = 0;
    for (auto it = o.begin(); it != o.end(); ++it) {
        if (!actionObjectFields().contains(it.key())) {
            *error = QStringLiteral("unknown action field \"%1\"").arg(it.key());
            return {};
        }
        if (it.key() != "label")
            ++kinds;
    }
    if (kinds != 1) {
        *error = QStringLiteral("action object needs exactly one of key/mod/layer/hide/text/command");
        return {};
    }
    a.label = o.value("label").toString();

    if (o.contains("text")) {
        a.type = Action::Type::Text;
        a.text = o.value("text").toString();
        if (a.text.isEmpty()) {
            *error = QStringLiteral("empty text action");
            return {};
        }
    } else if (o.contains("key")) {
        a.type = Action::Type::Key;
        a.keySpec = o.value("key").toString().toLower();
        const QStringList parts = a.keySpec.split('+', Qt::SkipEmptyParts);
        if (parts.isEmpty()) {
            *error = QStringLiteral("empty key spec");
            return {};
        }
        for (const QString &p : parts) {
            const int code = keyCodeByName(p.trimmed());
            if (code < 0) {
                *error = QStringLiteral("unknown key name \"%1\"").arg(p);
                return {};
            }
            a.keys << code;
        }
    } else if (o.contains("mod")) {
        a.type = Action::Type::Mod;
        if (!modFromName(o.value("mod").toString(), &a.mod)) {
            *error = QStringLiteral("unknown modifier \"%1\"").arg(o.value("mod").toString());
            return {};
        }
    } else if (o.contains("layer")) {
        a.type = Action::Type::Layer;
        a.layer = o.value("layer").toString();
        if (a.layer.isEmpty()) {
            *error = QStringLiteral("empty layer name");
            return {};
        }
    } else if (o.contains("hide")) {
        a.type = Action::Type::Hide;
    } else if (o.contains("command")) {
        a.type = Action::Type::Command;
        a.command = o.value("command").toString();
        if (a.command != "toggle-reserve" && a.command != "reset-position") {
            *error = QStringLiteral("unknown command \"%1\"").arg(a.command);
            return {};
        }
    }
    return a;
}

QString displayLabel(const Action &a, bool shifted)
{
    if (!a.label.isEmpty())
        return a.label;

    switch (a.type) {
    case Action::Type::None:
        return {};
    case Action::Type::Text: {
        if (a.text == " ") return QStringLiteral("␣");
        if (a.text == "\n") return QStringLiteral("⏎");
        if (a.text == "\t") return QStringLiteral("⇥");
        // Combining marks (e.g. U+0301) get a dotted circle to sit on.
        if (a.text.size() == 1 && a.text.at(0).category() == QChar::Mark_NonSpacing)
            return QStringLiteral("◌") + a.text;
        return shifted ? a.text.toUpper() : a.text;
    }
    case Action::Type::Key: {
        const QStringList parts = a.keySpec.split('+', Qt::SkipEmptyParts);
        QString out;
        for (int i = 0; i < parts.size(); ++i) {
            const QString p = parts.at(i).trimmed();
            if (i + 1 < parts.size() && modPrefixes().contains(p))
                out += modPrefixes().value(p);
            else
                out += prettyKeyNames().value(p, p);
        }
        return out;
    }
    case Action::Type::Mod:
        switch (a.mod) {
        case Mod::Shift: return QStringLiteral("⇧");
        case Mod::Ctrl: return QStringLiteral("Ctrl");
        case Mod::Alt: return QStringLiteral("Alt");
        case Mod::Super: return QStringLiteral("❖");
        }
        return {};
    case Action::Type::Layer:
        if (a.layer == "main") return QStringLiteral("abc");
        if (a.layer == "numeric") return QStringLiteral("123");
        return a.layer;
    case Action::Type::Hide:
        return QStringLiteral("▾");
    case Action::Type::Command:
        return a.command == "toggle-reserve" ? QStringLiteral("⇕") : QStringLiteral("⟲");
    }
    return {};
}

bool asciiToKey(QChar c, int *code, bool *needShift)
{
    *needShift = false;
    const ushort u = c.unicode();
    if (u >= 'a' && u <= 'z') {
        *code = keyCodeByName(QString(c));
        return true;
    }
    if (u >= 'A' && u <= 'Z') {
        *code = keyCodeByName(QString(c.toLower()));
        *needShift = true;
        return true;
    }
    if (u >= '0' && u <= '9') {
        *code = keyCodeByName(QString(c));
        return true;
    }
    struct M { char ch; int code; bool shift; };
    static const M map[] = {
        {' ', KEY_SPACE, false}, {'\n', KEY_ENTER, false}, {'\t', KEY_TAB, false},
        {'-', KEY_MINUS, false}, {'_', KEY_MINUS, true},
        {'=', KEY_EQUAL, false}, {'+', KEY_EQUAL, true},
        {'[', KEY_LEFTBRACE, false}, {'{', KEY_LEFTBRACE, true},
        {']', KEY_RIGHTBRACE, false}, {'}', KEY_RIGHTBRACE, true},
        {'\\', KEY_BACKSLASH, false}, {'|', KEY_BACKSLASH, true},
        {';', KEY_SEMICOLON, false}, {':', KEY_SEMICOLON, true},
        {'\'', KEY_APOSTROPHE, false}, {'"', KEY_APOSTROPHE, true},
        {'`', KEY_GRAVE, false}, {'~', KEY_GRAVE, true},
        {',', KEY_COMMA, false}, {'<', KEY_COMMA, true},
        {'.', KEY_DOT, false}, {'>', KEY_DOT, true},
        {'/', KEY_SLASH, false}, {'?', KEY_SLASH, true},
        {'!', KEY_1, true}, {'@', KEY_2, true}, {'#', KEY_3, true}, {'$', KEY_4, true},
        {'%', KEY_5, true}, {'^', KEY_6, true}, {'&', KEY_7, true}, {'*', KEY_8, true},
        {'(', KEY_9, true}, {')', KEY_0, true},
    };
    for (const M &m : map) {
        if (u == static_cast<ushort>(m.ch)) {
            *code = m.code;
            *needShift = m.shift;
            return true;
        }
    }
    return false;
}

} // namespace tp
