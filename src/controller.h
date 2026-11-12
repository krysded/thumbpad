#pragma once

#include "actions.h"
#include "renderer.h"

#include <QObject>
#include <array>

namespace tp {

class InputMethod;
class UInput;

// Executes actions and owns the modifier/layer state. One layout definition
// yields every mode: Shift uppercases text, and Ctrl/Alt/Super turn a letter
// into a real key chord, so there are no hand-written CTRLED/ALTED grids to
// keep in sync.
class Controller : public QObject
{
    Q_OBJECT
public:
    enum ModState { Off = 0, OneShot = 1, Locked = 2 };

    Controller(InputMethod *im, UInput *uinput, QObject *parent = nullptr);

    void execute(const Action &a);
    void tapKey(int keycode); // arrow keys from cursor slide; honours active mods

    QString layer() const { return m_layer; }
    void setLayer(const QString &layer);
    bool shifted() const { return m_mods[int(Mod::Shift)] != Off; }
    RenderState renderState() const;
    QString stateString() const;

Q_SIGNALS:
    void stateChanged();
    void hideRequested();
    void commandRequested(const QString &command);

private:
    void commitText(const QString &text);
    QList<int> activeModKeys(bool includeShift) const;
    void consumeOneShots();

    InputMethod *m_im;
    UInput *m_uinput;
    QString m_layer = QStringLiteral("main");
    std::array<ModState, ModCount> m_mods{};
};

} // namespace tp
