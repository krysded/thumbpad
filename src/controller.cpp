#include "controller.h"
#include "inputmethod.h"
#include "uinput.h"

#include <linux/input-event-codes.h>

namespace tp {

namespace {
const int kModKeys[ModCount] = {KEY_LEFTSHIFT, KEY_LEFTCTRL, KEY_LEFTALT, KEY_LEFTMETA};
}

Controller::Controller(InputMethod *im, UInput *uinput, QObject *parent)
    : QObject(parent), m_im(im), m_uinput(uinput)
{
}

void Controller::setLayer(const QString &layer)
{
    if (m_layer != layer) {
        m_layer = layer;
        Q_EMIT stateChanged();
    }
}

QList<int> Controller::activeModKeys(bool includeShift) const
{
    QList<int> keys;
    for (int i = 0; i < ModCount; ++i) {
        if (i == int(Mod::Shift) && !includeShift)
            continue;
        if (m_mods[i] != Off)
            keys << kModKeys[i];
    }
    return keys;
}

void Controller::consumeOneShots()
{
    bool changed = false;
    for (ModState &m : m_mods) {
        if (m == OneShot) {
            m = Off;
            changed = true;
        }
    }
    if (changed)
        Q_EMIT stateChanged();
}

void Controller::commitText(const QString &text)
{
    if (m_im && m_im->hasContext()) {
        m_im->commitText(text);
        return;
    }
    // No text field session (e.g. started outside KWin, or an app without
    // text-input support): fall back to typing ASCII via uinput.
    for (const QChar c : text) {
        if (!m_uinput || !m_uinput->typeChar(c))
            qWarning("thumbpad: cannot type \"%s\" without an input-method session",
                     qPrintable(QString(c)));
    }
}

void Controller::tapKey(int keycode)
{
    if (!m_uinput)
        return;
    QList<int> keys = activeModKeys(true);
    keys << keycode;
    m_uinput->chord(keys);
    // Deliberately no consumeOneShots(): Shift+slide keeps selecting.
}

void Controller::execute(const Action &a)
{
    switch (a.type) {
    case Action::Type::None:
        return;

    case Action::Type::Mod: {
        ModState &m = m_mods[int(a.mod)];
        m = m == Off ? OneShot : m == OneShot ? Locked : Off;
        Q_EMIT stateChanged();
        return;
    }

    case Action::Type::Layer:
        setLayer(a.layer);
        return;

    case Action::Type::Hide:
        Q_EMIT hideRequested();
        return;

    case Action::Type::Command:
        Q_EMIT commandRequested(a.command);
        return;

    case Action::Type::Key: {
        if (!m_uinput || !m_uinput->isOpen()) {
            qWarning("thumbpad: key action \"%s\" needs uinput", qPrintable(a.keySpec));
            return;
        }
        QList<int> keys = activeModKeys(true);
        for (int k : a.keys)
            if (!keys.contains(k))
                keys << k;
        m_uinput->chord(keys);
        consumeOneShots();
        return;
    }

    case Action::Type::Text: {
        const bool command = m_mods[int(Mod::Ctrl)] != Off || m_mods[int(Mod::Alt)] != Off ||
                             m_mods[int(Mod::Super)] != Off;
        int code;
        bool needShift;
        if (command && a.text.size() == 1 && m_uinput && m_uinput->isOpen() &&
            asciiToKey(a.text.at(0), &code, &needShift)) {
            // Ctrl+letter etc.: a real chord, e.g. Ctrl+Shift+C copies in Konsole.
            QList<int> keys = activeModKeys(false);
            if (needShift || shifted())
                keys << KEY_LEFTSHIFT;
            keys << code;
            m_uinput->chord(keys);
        } else {
            commitText(shifted() ? a.text.toUpper() : a.text);
        }
        consumeOneShots();
        return;
    }
    }
}

RenderState Controller::renderState() const
{
    RenderState st;
    st.shifted = shifted();
    for (int i = 0; i < ModCount; ++i)
        st.mods[i] = m_mods[i];
    return st;
}

QString Controller::stateString() const
{
    QStringList mods;
    for (int i = 0; i < ModCount; ++i)
        if (m_mods[i] != Off)
            mods << modName(Mod(i)) + (m_mods[i] == Locked ? "(locked)" : "");
    return QStringLiteral("layer=%1 mods=%2 im=%3")
        .arg(m_layer, mods.isEmpty() ? "-" : mods.join(','),
             m_im && m_im->hasContext() ? "active" : "inactive");
}

} // namespace tp
