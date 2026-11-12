#pragma once

#include <QObject>
#include <functional>

namespace tp {

class Controller;
class KeyboardUi;

inline constexpr char kDBusService[] = "io.github.krysded.thumbpad";
inline constexpr char kDBusPath[] = "/";
inline constexpr char kDBusInterface[] = "io.github.krysded.thumbpad";

// Session-bus control, e.g. for a panel button or a touch gesture:
//   qdbus6 io.github.krysded.thumbpad / Toggle
//   qdbus6 io.github.krysded.thumbpad / Press '{"key":"ctrl+a"}'
class DBusIface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.krysded.thumbpad")
public:
    DBusIface(Controller *c, KeyboardUi *ui, std::function<void()> reload, QObject *parent);

public Q_SLOTS:
    void Toggle();
    void Show();
    void Hide();
    void Reload();
    void ToggleReserve();
    void ResetPositions();
    void MoveBlock(int index, double dx, double dy);
    // Runs one action given as JSON: "\"a\"" commits text, '{"key":"esc"}' a key.
    QString Press(const QString &actionJson);
    QString State();

private:
    Controller *m_controller;
    KeyboardUi *m_ui;
    std::function<void()> m_reload;
};

} // namespace tp
